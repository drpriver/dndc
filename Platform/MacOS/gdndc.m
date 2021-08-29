#import <Cocoa/Cocoa.h>
#import <Webkit/WebKit.h>
#import <dispatch/dispatch.h>
#define DNDC_API static inline
#import "dndc_long_string.h"
#import "common_macros.h"
#import "measure_time.h"
#import "dndc.h"
#import "MStringBuilder.h"
#import "mallocator.h"
#import "msb_format.h"
#import "dndc_funcs.h"
#define LOGIT(...) NSLog(@ "%d: " #__VA_ARGS__ "= %@", __LINE__, __VA_ARGS__)
// Convenience macro for writing inline javascript without a million quotes.
// Note that you need to semi-colon terminate all of your lines.
#define JSRAW(...) #__VA_ARGS__

#if !__has_feature(objc_arc)
#error "ARC is off"
#endif

#pragma clang assume_nonnull begin

static
NSString*_Nonnull
msb_detach_as_ns_string(MStringBuilder*sb){
    auto text = msb_detach(sb);
    PushDiagnostic();
    SuppressCastQual();
    NSData* data = [NSData dataWithBytesNoCopy:(void*)text.text length:text.length freeWhenDone:YES];
    PopDiagnostic();
    NSString* str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return str;
}

static DndcFileCache*_Nonnull BASE64CACHE;
static DndcFileCache*_Nonnull TEXTCACHE;
typedef struct FileWatchItem {
    uint64_t hash;
    uint64_t last_eight_chars;
    LongString fullpath;
    int fd;
    bool tomb;
} FileWatchItem;
typedef struct FileWatchCache {
    size_t capacity;
    size_t count;
    FileWatchItem* items;
} FileWatchCache;

// cut'n'paste from the wikipedia page on murmur hash
static inline
force_inline
nosan
uint32_t
murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

static inline
force_inline
nosan
uint32_t
murmur3_32(Nonnull(const uint8_t*) key, size_t len, uint32_t seed)
{
	uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    h ^= murmur_32_scramble(k);
    /* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

static
void
cache_watch_file(void* cache_, StringView path){
    // path is not necessarily valid - python blocks can add dependencies.
    FileWatchCache* cache = cache_;
    if(!path.length || !path.text){
        NSLog(@"Not watching invalid path");
        return;
    }
    uint64_t hash = murmur3_32((const uint8_t*)path.text, path.length, 1107845655llu);
    uint64_t last_eight = 0;
    const char* end = path.text + path.length;
    size_t length = path.length >= 8? 8 : path.length;
    memcpy(&last_eight, end-length, length);
    // TODO: use a hash table.
    size_t first_tomb = -1;
    for(size_t i = 0; i < cache->count; i++){
        auto it = &cache->items[i];
        if(it->tomb){
            if(first_tomb == (size_t)-1){
                first_tomb = i;
            }
            continue;
        }
        if(it->hash == hash){
            if(it->last_eight_chars == last_eight){
                if(LS_SV_equals(it->fullpath, path)){
                    // already watching it.
                    return;
                }
            }
        }
    }
    if(cache->count >= cache->capacity){
        size_t capacity = cache->capacity * 2;
        if(!capacity)
            capacity = 8;
        FileWatchItem* items = realloc(cache->items, capacity*sizeof(*items));
        if(!items){
            NSLog(@"Resizing filewatchcache to %zu items failed", capacity);
            return;
        }
        cache->items = items;
        cache->capacity = capacity;
    }
    auto item_index = (first_tomb != (size_t)-1)?first_tomb:cache->count++;
    auto item = &cache->items[item_index];
    item->tomb = false;
    item->hash = hash;
    item->last_eight_chars = last_eight;
    item->fullpath.text = strndup(path.text, path.length);
    if(!item->fullpath.text){
        NSLog(@"strndup failed");
        item->tomb = true;
        return;
    }
    item->fullpath.length = path.length;
    item->fd = open(item->fullpath.text, O_EVTONLY);
    if(item->fd < 0){
        NSLog(@"open call for '%s' failed: %s", item->fullpath.text, strerror(errno));
        item->tomb = true;
        const_free(item->fullpath.text);
        return;
    }
    // NSLog(@"watching '%s'", item->fullpath.text);
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, item->fd, DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_ATTRIB, dispatch_get_main_queue());
    dispatch_source_set_event_handler(source, ^{
        auto bitem = &cache->items[item_index];
        dndc_filecache_remove(TEXTCACHE, LS_to_SV(bitem->fullpath));
        dndc_filecache_remove(BASE64CACHE, LS_to_SV(bitem->fullpath));
        NSLog(@"'%s' changed", bitem->fullpath.text);
#if 0
        uint64_t mask = dispatch_source_get_data(source);
        if(mask & DISPATCH_VNODE_DELETE){
            NSLog(@"VNode for '%s' was deleted", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_RENAME){
            NSLog(@"VNode for '%s' was renamed", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_WRITE) {
            NSLog(@"VNode for '%s' was written", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_EXTEND) {
            NSLog(@"VNode for '%s' was extended", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_ATTRIB) {
            NSLog(@"VNode for '%s' metadata changed", bitem->fullpath.text);
        }
#endif
        dispatch_source_cancel(source);
    });
    dispatch_source_set_cancel_handler(source, ^{
        auto bitem = &cache->items[item_index];
        // NSLog(@"VNode for '%s' was canceled", bitem->fullpath.text);
        bitem->tomb = true;
        close(bitem->fd);
        const_free(bitem->fullpath.text);
    });
    dispatch_resume(source);
}

static NSFont* EDITOR_FONT;

static FileWatchCache FILE_WATCH_CACHE;

static
int
cache_watch_files(void* unused, size_t npaths, StringView*paths){
    (void)unused;
    for(size_t i = 0; i < npaths; i++){
        cache_watch_file(&FILE_WATCH_CACHE, paths[i]);
    }
    return 0;
}

//
// So, each document has N window controllers (I guess 1 for me).
// Each window controller has a window.
// Each window has a viewcontroller.
// Each viewcontroller will have a webview and a textview.
// The textview has a storage which has a highlighter.
// The webview has a webnavdel.
//
// Who owns the documents though? The DocController??
//

//
// Tags for inserting img blocks
typedef enum GdndInsertTag{
    GDND_INSERT_IMG,
    GDND_INSERT_IMGLINKS,
    GDND_INSERT_SCRIPT,
    GDND_INSERT_CSS,
    GDND_INSERT_DND,
}GdndInsertTag;
@interface DndWindowController: NSWindowController
// Has a NSWindow* window
@end

@interface DndFontDelegate: NSObject<NSWindowDelegate>
-(void)changeFont:(nullable id)sender;
@end

//
// Customized text view
// Supports indent, dedent, inserting special blocks,
// uses plain text for paste, smart indent, smart tab with smart backspace.
//
@interface DndTextView: NSTextView
-(void)insert_file_block:(NSString*)path tag:(GdndInsertTag)tag size:(NSSize)size;
@end

@class DndViewController;
//
// Delegate for the above textview that provides syntax highlighting
// for the .dnd file
//
@interface DndHighlighter: NSObject<NSTextStorageDelegate>
@property(weak, nonatomic) DndViewController* controller;
@end

//
// Controls what urls to allow (basically makes it so links will open a new
// .dnd document)
//
@interface WebNavDel : NSObject <WKNavigationDelegate>
@property(weak, nonatomic) DndViewController* controller;
@end

@interface DndUrlHandler: NSObject <WKURLSchemeHandler>
@property(weak, nonatomic) DndViewController* controller;
@end

//
// ViewController for the windows of the app
//
@interface DndViewController: NSViewController <WKUIDelegate>{
@public DndTextView* text;
@public NSScrollView* scrollview; // contains the text
@public NSSplitView* editor_container;
@public NSTextView* error_text;
@public DndHighlighter* highlighter; // for the text
@public WKWebView* webview;
@public WebNavDel* webnavdel; // for the webview
@public NSURL* file_url;
@public DndUrlHandler* handler;
@public NSString* scroll_resto_string;
@public NSString* doc_title;
}
-(LongString)get_text;
-(void)recalc_html:(LongString)text;
-(void)flop_editor:(id _Nullable)sender;
@end

//
// The Dnd document
//
@interface DndDocument: NSDocument{
// this is kind of janky, but whatever
DndViewController* view_controller;
}
-(void)change_font:(NSFont*)font;
@end

// The App delegate!
@interface DndAppDelegate : NSObject<NSApplicationDelegate>
@end

//
// Setup menus without needing a nib (xib? whatever).
//
static void do_menus(void);
//
// This is for detecting indent for smart indent
static NSString * const kIndentPatternString = @"^(\\t|\\s)+";
// ditto
static NSRegularExpression* indent_pattern;
//
// The app's image. We embed the png into the binary and decode it at startup.
static NSImage* appimage;


@implementation DndDocument
+(BOOL)autosavesInPlace {
    return YES;
}
-(void)change_font:(NSFont*)font{
    self->view_controller->text.font = font;
}
-(instancetype)init {
    self = [super init];
    self->view_controller = [[DndViewController alloc] init];
    return self;
}
- (instancetype _Nullable)initForURL:(NSURL *_Nullable)urlOrNil
    withContentsOfURL:(NSURL *)contentsURL
    ofType:(NSString *)typeName
    error:(NSError * _Nullable *)outError{
    self = [super initForURL:urlOrNil withContentsOfURL:contentsURL ofType:typeName error:outError];
    view_controller->file_url = [self fileURL];
    [view_controller recalc_html:[view_controller get_text]];
    return self;
    }
-(NSWindow*)make_window{
    auto screen = [NSScreen mainScreen];
    NSRect rect = screen? screen.visibleFrame : NSMakeRect(0, 0, 1400, 800);

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect: rect
        styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
        backing: NSBackingStoreBuffered
        defer: NO];
    window.title = @"Dndc";
    window.tabbingIdentifier = @"Dnd Window";
    window.contentViewController = self->view_controller;
    return window;
}
-(void)makeWindowControllers{
    NSWindow* docwindow = [self make_window];
    DndWindowController* winc = [[DndWindowController alloc] initWithWindow:docwindow];
    [self addWindowController:winc];
    auto mainwindow = [NSApp mainWindow];
    if(mainwindow){
        [mainwindow addTabbedWindow:docwindow ordered:NSWindowAbove];
    }
    [docwindow makeKeyAndOrderFront: nil];
}
-(NSString*)doc_title{
    return self->view_controller->doc_title;
}

- (NSData*_Nullable)dataOfType:(NSString *)typeName error:(NSError **)outError {
    return [[view_controller->text string] dataUsingEncoding:NSUTF8StringEncoding];
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError {
    NSString* str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if(str){
        view_controller->file_url = [self fileURL];
        view_controller->text.string = str;
    }
    return YES;
}
@end

static NSColor*_Nonnull SYNTAX_COLORS[DNDC_SYNTAX_MAX] = {};
struct SyntaxData {
    NSTextStorage* storage;
    const uint16_t* begin;
    const uint16_t* begin_edited_line;
    const uint16_t* end_edited_lines;
};
static
void
dndc_syntax_func(void* _Nullable data, int type, int line, int col, Nonnull(const uint16_t*)begin, size_t length){
    (void)line;
    (void)col;
    struct SyntaxData* sd = data;
    if(begin + length < sd->begin_edited_line)
        return;
    if(begin > sd->end_edited_lines)
        return;
    if(type == DNDC_SYNTAX_RAW_STRING)
        return;
    [sd->storage addAttribute:NSForegroundColorAttributeName value:SYNTAX_COLORS[type] range:NSMakeRange(begin-sd->begin, length)];
    return;
}
static
int
gdndc_ast_func(void*_Nullable data, DndcContext* ctx){
    if(!data)
        return 0;
    if(!NodeHandle_eq(ctx->titlenode, INVALID_NODE_HANDLE)){
        Node* node = get_node(ctx, ctx->titlenode);
        MStringBuilder sb = {.allocator = get_mallocator()};
        msb_write_str(&sb, node->header.text, node->header.length);
        NSString* str = msb_detach_as_ns_string(&sb);
        DndViewController* vc = (__bridge DndViewController*)data;
        vc->doc_title = str;
    }
    return 0;
}
static
void
gdndc_error_func(void* _Nullable data, int type, const char*_Nonnull filename, int filename_len, int line, int col, const char*_Nonnull message, int message_len){
    if(!data)
        return;
    NSTextView* tv = (__bridge NSTextView*)data;
    MStringBuilder builder = {.allocator=get_mallocator()};
    StringView fn = {
        .text = filename,
        .length = filename_len,
    };
    StringView mess = {
        .text = message,
        .length = message_len,
    };
    switch((enum DndcErrorMessageType)type){
        case DNDC_ERROR_MESSAGE:
        case DNDC_WARNING_MESSAGE:
            if(SV_equals(fn, SV("(string input)"))){
                MSB_FORMAT(&builder, line, ":", col, ": ", mess, "\n");
            }
            else{
                MSB_FORMAT(&builder, fn, ":", line, ":", col, ": ", mess, "\n");
            }
            break;
        case DNDC_NODELESS_MESSAGE:
        case DNDC_STATISTIC_MESSAGE:
        case DNDC_DEBUG_MESSAGE:
            MSB_FORMAT(&builder, mess, "\n");
            break;
    }
    auto s = msb_detach_as_ns_string(&builder);
    [[tv textStorage].mutableString appendString:s];
}

@implementation DndHighlighter
- (void)textStorage:(NSTextStorage *)textStorage
  didProcessEditing:(NSTextStorageEditActions)editedMask
              range:(NSRange)editedRange
     changeInLength:(NSInteger)delta{
    NSString *string = textStorage.string;
    LongString text;
    text.text = [string UTF8String];
    text.length = strlen(text.text);
    if(self.controller){
        [self.controller recalc_html: [self.controller get_text]];
    }
    // NSRange currentLineRange = NSMakeRange(0, [string length]);
    NSRange currentLineRange = [string lineRangeForRange:editedRange];
    // auto before= get_t();
    [textStorage removeAttribute:NSForegroundColorAttributeName range:currentLineRange];
    [textStorage removeAttribute:NSBackgroundColorAttributeName range:currentLineRange];
    // HERE("Clearing syntax costs: %.3fms", (get_t()-before)/1000.);
    auto len = [string length];
    // gross!
    static unichar* chars;
    static size_t chars_length;
    if(chars_length < len){
        chars = realloc(chars, sizeof(*chars)*len);
        chars_length = len;
    }
    [string getCharacters:chars range:NSMakeRange(0, len)];
    struct SyntaxData sd = {
        .storage = textStorage,
        .begin = chars,
        .begin_edited_line = currentLineRange.location + chars,
        .end_edited_lines = currentLineRange.location+currentLineRange.length+chars,
    };
    StringViewUtf16 text16 = {
        .text = chars,
        .length = len,
    };
    // auto t0 = get_t();
    // for(int i = 0; i < 1000; i++)
        dndc_analyze_syntax_utf16(text16, dndc_syntax_func, &sd);
    // auto t1 = get_t();
    // HERE("dndc_analyze_syntax: %.3fms", (t1-t0)/1000.);
    return;
}
@end

@implementation DndTextView
-(void)keyDown:(NSEvent*) event{
    if(event.modifierFlags & NSEventModifierFlagCommand){
        auto num = [event.characters integerValue];
        if(num){
            num -= 1;
           auto tabs =  [NSApp mainWindow].tabbedWindows;
           if(tabs.count > num){
               [tabs[num] makeKeyAndOrderFront:nil];
               return;
           }
        }
    }
    [super keyDown:event];
}
-(void)indent:(id _Nullable)sender{
    auto r = self.selectedRange;
    NSRange currentLineRange = [self.string lineRangeForRange:r];
    int adjustment = 0;
    [self insertText:@"  " replacementRange:NSMakeRange(currentLineRange.location, 0)];
    adjustment += 2;
    //               -1 to not do final newline
    for(int i = 0; i < currentLineRange.length-1; i++){
        auto c = [self.string characterAtIndex:i+currentLineRange.location+adjustment];
        if(c == '\n'){
            [self insertText:@"  " replacementRange:NSMakeRange(currentLineRange.location + i + 1 + adjustment, 0)];
            adjustment += 2;
        }
    }
    auto adjustedrange = NSMakeRange(currentLineRange.location, currentLineRange.length+adjustment);
    if(r.length > 0)
        [self setSelectedRange:adjustedrange];
}
-(void)dedent:(id _Nullable)sender{
    auto r = self.selectedRange;
    NSRange currentLineRange = [self.string lineRangeForRange:r];
    int adjustment = 0;
    int n_leading_space = 0;  // negative means no longer counting leading spaces.
    for(int i = 0; i < currentLineRange.length-1; i++){
        auto c = [self.string characterAtIndex:i+currentLineRange.location+adjustment];
        if(c == '\n'){
            n_leading_space = 0;
            continue;
        }
        if(n_leading_space < 0){
            continue;
        }
        if(c == ' '){
            n_leading_space++;
            if(n_leading_space == 2){
                auto range = NSMakeRange(currentLineRange.location+adjustment+i-1, 2);
                [self insertText:@"" replacementRange:range];
                n_leading_space = -1;
                adjustment -= 2;
            }
            continue;
        }
        n_leading_space = -1;
    }
    auto adjustedrange = NSMakeRange(currentLineRange.location, currentLineRange.length+adjustment);
    if(r.length > 0)
        [self setSelectedRange:adjustedrange];
}
-(void)ensure_pattern{
    if(!indent_pattern)
        PushDiagnostic();
        #pragma clang diagnostic ignored "-Wnullable-to-nonnull-conversion"
        indent_pattern = [[NSRegularExpression alloc] initWithPattern:kIndentPatternString options:0 error:nil];
        PopDiagnostic();
}
-(void)insert_file_block:(NSString*)path tag:(GdndInsertTag)tag size:(NSSize)size{
    auto r = self.selectedRange;
    NSRange currentLineRange = [self.string lineRangeForRange:r];
    auto rel = r.location - currentLineRange.location;
    switch(tag){
        case GDND_INSERT_IMG:
            [self insert_block:path at:r indent_amount:rel name:SV("::img\n")];
            break;
        case GDND_INSERT_DND:
            [self insert_block:path at:r indent_amount:rel name:SV("::import\n")];
            break;
        case GDND_INSERT_SCRIPT:
            [self insert_block:path at:r indent_amount:rel name:SV("::script\n")];
            break;
        case GDND_INSERT_CSS:
            [self insert_block:path at:r indent_amount:rel name:SV("::css\n")];
            break;
        case GDND_INSERT_IMGLINKS:
            [self insert_imglinks_block:path at:r indent_amount:rel size:size];
            break;
        PushDiagnostic();
        SuppressCoveredSwitchDefault();
        default:
            NSLog(@"%s: Unknown insert_file_block tag given: %d", __func__, tag);
            break;
        PopDiagnostic();
    }
}

-(void)insert_block:(NSString*)path at:(NSRange)r indent_amount:(NSInteger)indent_amount name:(StringView)blockname{
    MStringBuilder sb = {.allocator=get_mallocator()};
    msb_ensure_additional(&sb, 256);
    msb_write_str(&sb, blockname.text, blockname.length);
    msb_write_nchar(&sb, ' ', indent_amount+2);
    const char* cpath = [path UTF8String];
    msb_write_str(&sb, cpath, strlen(cpath));
    msb_write_char(&sb, '\n');
    auto to_insert = msb_detach_as_ns_string(&sb);
    [self insertText:to_insert replacementRange:r];
}
-(void)insert_imglinks_block:(NSString*)path at:(NSRange)r indent_amount:(NSInteger)indent_amount size:(NSSize)size{
    MStringBuilder sb = {.allocator = get_mallocator()};
    msb_ensure_additional(&sb, 256);
    msb_write_literal(&sb, "::imglinks\n");
#define INDENT() msb_write_nchar(&sb, ' ', indent_amount+2)
    const char* imgpath = [path UTF8String];
    INDENT(); msb_write_str(&sb, imgpath, strlen(imgpath));
    msb_write_char(&sb, '\n');
    double scale = size.width > size.height? 800.0/size.width : 800.0/size.height;
    INDENT(); MSB_FORMAT(&sb, "width = ", (int)(size.width*scale), "\n");
    INDENT(); MSB_FORMAT(&sb, "height = \n", (int)(size.height*scale), "\n");
    INDENT(); MSB_FORMAT(&sb, "viewBox = 0 0 ", (int)size.width, " ", (int)size.height, "\n");
    StringView script[] = {
        SV("::python\n"),
        SV("  # this is an example of how to script the imglinks\n"),
        SV("  imglinks = node.parent\n"),
        SV("  coord_nodes = ctx.select_nodes(attributes=['coord'])\n"),
        SV("  for c in coord_nodes:\n"),
        SV("    lead = c.header  # change this probably\n"),
        SV("    position = c.attributes['coord']\n"),
        SV("    imglinks.add_child(f'{lead} = {ctx.outfile}#{c.id} @{position}')\n"),
        SV("  #endpython\n"),
    };
    for(int i = 0; i < arrlen(script); i++){
        INDENT();
        msb_write_str(&sb, script[i].text, script[i].length);
    }
#undef INDENT
    auto to_insert = msb_detach_as_ns_string(&sb);
    [self insertText:to_insert replacementRange:r];
}
+(NSMenu*_Nullable)defaultMenu{
    NSMenu* result = [[NSMenu alloc] initWithTitle:@"Menu"];
    NSMenuItem* item;
    item = [[NSMenuItem alloc] initWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@""];
    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@""];
    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@""];
    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Indent" action:@selector(indent:) keyEquivalent:@""];
    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Dedent" action:@selector(dedent:) keyEquivalent:@""];
    [result addItem:item];
    [result addItem:[NSMenuItem separatorItem]];

    item = [[NSMenuItem alloc] initWithTitle:@"Insert Image" action:@selector(insert_file:) keyEquivalent:@""];
    item.tag = GDND_INSERT_IMG;

    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Insert Imglinks" action:@selector(insert_file:) keyEquivalent:@""];
    item.tag = GDND_INSERT_IMGLINKS;

    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Insert CSS" action:@selector(insert_file:) keyEquivalent:@""];
    item.tag = GDND_INSERT_CSS;

    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Insert Script" action:@selector(insert_file:) keyEquivalent:@""];
    item.tag = GDND_INSERT_SCRIPT;

    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Insert Import" action:@selector(insert_file:) keyEquivalent:@""];
    item.tag = GDND_INSERT_DND;

    [result addItem:item];
    [result addItem:[NSMenuItem separatorItem]];
    return result;
}
-(NSString *_Nullable)preferredPasteboardTypeFromArray:(NSArray *)availableTypes restrictedToTypesFromArray:(NSArray *_Nullable)allowedTypes {
  if ([availableTypes containsObject:NSPasteboardTypeString]) {
    return NSPasteboardTypeString;
  }

  return [super preferredPasteboardTypeFromArray:availableTypes restrictedToTypesFromArray:allowedTypes];
}
-(DndTextView*)initWithFrame:(NSRect)textrect font:(NSFont*)font{
    self = [super initWithFrame:textrect];
    [self ensure_pattern];
    self.usesAdaptiveColorMappingForDarkAppearance = YES;
    self.automaticTextCompletionEnabled = NO;
    self.automaticLinkDetectionEnabled = NO;
    self.automaticSpellingCorrectionEnabled = NO;
    self.automaticDashSubstitutionEnabled = NO;
    self.automaticQuoteSubstitutionEnabled = NO;
    self.automaticDataDetectionEnabled = NO;
    self.font = font;
    self.allowsUndo = YES;
    // self.usesFindBar = YES;
    return self;
}

-(void)deleteBackward:(id _Nullable)sender{
    auto r = self.selectedRange;
    if(r.length == 0){
        NSRange currentLineRange = [self.string lineRangeForRange:r];
        NSString* currentLine = [self.string substringWithRange:currentLineRange];
        [self ensure_pattern];
        NSTextCheckingResult* indent_matched = [indent_pattern firstMatchInString:currentLine options:0 range:NSMakeRange(0, currentLine.length)];
        if(indent_matched){
            auto this_char = r.location?[self.string characterAtIndex:r.location-1]:'\0';
            // how far into the line the current selection is
            auto rel = r.location - currentLineRange.location;
            if(rel and !(rel & 1) and this_char == ' ' and rel <= indent_matched.range.length){
                    [super deleteBackward:sender];
                    [super deleteBackward:sender];
                    [self display];
                    return;
                }
            }
        }
    [super deleteBackward:sender];
    [self display];
}
-(void)insertNewline:(id _Nullable)sender {
    auto sel_range = self.selectedRange;
    if(sel_range.length){
        [super insertNewline:sender];
        return;
    }
    NSRange currentLineRange = [self.string lineRangeForRange:sel_range];
    NSString* currentLine = [self.string substringWithRange:currentLineRange];
    [super insertNewline:sender];
    if(currentLine.length){
        [self ensure_pattern];
        NSTextCheckingResult* indent_matched = [indent_pattern firstMatchInString:currentLine options:0 range:NSMakeRange(0, currentLine.length-1)];
        if (indent_matched) {
            NSString *indent = [currentLine substringWithRange:indent_matched.range];
            bool all_spaces = (indent_matched.range.length == currentLine.length-1);
            if(all_spaces){
                [self insertText:@"\n" replacementRange:currentLineRange];
            }

            [self insertText:indent replacementRange:self.selectedRange];
        }
    }
    [self display];
    [self.enclosingScrollView display];
}
- (void)insertTab:(id _Nullable)sender{
 [self insertText: @"  " replacementRange:self.selectedRange];
}

@end
#define DND_THIS_URL @"dnd://gdndc/this.html"
@implementation WebNavDel: NSObject
-(void)webView:(WKWebView *)webView
    decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler{
        auto path = navigationAction.request.URL.relativePath;
        if([path isEqual:@"/this.html"]){
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }
        if([path characterAtIndex:0] == '/'){
            auto real_url = [self.controller->file_url.URLByDeletingLastPathComponent URLByAppendingPathComponent:[path substringFromIndex:1]];
            real_url = [[real_url URLByDeletingPathExtension] URLByAppendingPathExtension:@"dnd"];
            [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:real_url display:YES completionHandler:^(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error){
                (void)documentWasAlreadyOpen;
                (void)document;
                if(error){
                    NSLog(@"%@", error);
                    decisionHandler(WKNavigationActionPolicyAllow);
                    }
                else {
                    decisionHandler(WKNavigationActionPolicyCancel);
                    }
                }];
            return;
        }
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
}
@end

@implementation DndUrlHandler
-(void)webView:(WKWebView*)webView startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask{
    auto request = [urlSchemeTask request];
    NSURL* url = request.URL;
    auto method = [request HTTPMethod];
    if([method isEqualToString:@"POST"] and [url isEqual:[NSURL URLWithString:@"dnd:///roomclick"]]){
        auto response = [[NSURLResponse alloc]
            initWithURL:(NSURL*)[NSURL URLWithString:DND_THIS_URL]
            MIMEType:@"text/plain"
            expectedContentLength:0
            textEncodingName:nil];
        [urlSchemeTask didReceiveResponse:response];
        NSString* body = [[NSString alloc] initWithData:(NSData*)[request HTTPBody] encoding:NSUTF8StringEncoding];
        [urlSchemeTask didFinish];
        [self.controller->text insertText:body replacementRange:NSMakeRange([[self.controller->text textStorage] length], 0)];
        return;
    }
    if([method isEqualToString:@"GET"]){
        if([[url scheme] isEqualToString:@"dnd"] and [[url path] isEqualToString:@"/scrollresto"]){
        auto response = [[NSHTTPURLResponse alloc]
            initWithURL:(NSURL*)[NSURL URLWithString:@"dnd://gdndc"]
             statusCode:200
            HTTPVersion:@"HTTP/1.1"
           headerFields:@{
           @"Access-Control-Allow-Origin":@"dnd://gdndc",
           @"Cache-Control":@"no-cache",
           }
        ];

        [urlSchemeTask didReceiveResponse:response];
        if(self.controller->scroll_resto_string){
            [urlSchemeTask didReceiveData: (NSData*)[self.controller->scroll_resto_string dataUsingEncoding:NSUTF16StringEncoding]];
        }
        [urlSchemeTask didFinish];
        return;
        }
    }
    // This is faster, but I can't figure out how to clear the image from the cache.
#if 0
    else if([method isEqualToString:@"GET"] and [[url scheme] isEqualToString:@"dnd"]){
        auto path = [url path];
        auto data = [NSData dataWithContentsOfFile: path];
        if(!data){
            // TODO: better error.
            [urlSchemeTask didFailWithError:[NSError errorWithDomain:@"denied" code:1 userInfo:nil]];
        }
        auto response = [[NSURLResponse alloc]
            initWithURL:(NSURL*)[NSURL URLWithString:DND_THIS_URL]
            MIMEType:@"image/png" // This is wrong, but webkit doesn't mind.
            expectedContentLength: [data length]
            textEncodingName:nil];
        [urlSchemeTask didReceiveResponse:response];
        [urlSchemeTask didReceiveData:data];
        [urlSchemeTask didFinish];
        int fd = open([path UTF8String], O_EVTONLY);
        if(fd > 0){
            dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fd, DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_ATTRIB, dispatch_get_main_queue());
            dispatch_source_set_event_handler(source, ^{
                    // TODO: figure out how to invalidate the webview cache.
                    dispatch_cancel(source);
                });
            dispatch_source_set_cancel_handler(source, ^{
                    close(fd);
                    });
            dispatch_resume(source);
        }

    }
#endif
    [urlSchemeTask didFailWithError:[NSError errorWithDomain:@"denied" code:1 userInfo:nil]];
}
-(void)webView:(WKWebView*)webView stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask{
}
@end


@implementation DndViewController {
BOOL editor_on_left;
BOOL auto_recalc;
BOOL coord_helper;
BOOL show_errors;
BOOL show_stats;
}
#define DND_AUTO_APPLY_CHANGES_LABEL @"Auto-Apply Changes"
#define DND_READ_ONLY_LABEL @"Read-Only"
#define DND_COORD_HELPER_LABEL @"Coord Helper"
#define DND_SHOW_ERRORS_LABEL @"Show Errors"
#define DND_SHOW_STATS_LABEL @"Show Stats"
-(void)refresh{
    [self recalc_html:[self get_text]];
}
-(void)button_click:(id)a{
    // Being lazy and just doing string comparisons
    NSButton* button = a;
    auto title = [button title];
    auto state = [button state];
    if([title isEqualToString:DND_AUTO_APPLY_CHANGES_LABEL]){
        if(state == NSControlStateValueOn){
            auto_recalc = YES;
            [self recalc_html:[self get_text]];
        }
        else {
            auto_recalc = NO;
        }
    }
    else if([title isEqualToString:DND_READ_ONLY_LABEL]){
        if(state == NSControlStateValueOn){
            self->text.editable = NO;
        }
        else {
            self->text.editable = YES;
        }
    }
    else if([title isEqualToString:DND_COORD_HELPER_LABEL]){
        if(state == NSControlStateValueOn){
            coord_helper = YES;
            [self recalc_html:[self get_text]];
        }
        else {
            coord_helper = NO;
            [self recalc_html:[self get_text]];
        }
    }
    else if([title isEqualToString:DND_SHOW_ERRORS_LABEL]){
        if(state == NSControlStateValueOn){
            show_errors = YES;
            [self recalc_html:[self get_text]];
        }
        else {
            show_errors = NO;
            [self recalc_html:[self get_text]];
        }
    }
    else if([title isEqualToString:DND_SHOW_STATS_LABEL]){
        if(state == NSControlStateValueOn){
            show_stats = YES;
        }
        else {
            show_stats = NO;
        }
    }
    else {
        NSLog(@"Unknown button title:%@", title);
    }
}
-(instancetype)init{
    self = [super init];
    editor_on_left = NO;
    coord_helper = NO;
    auto_recalc = YES;
    show_errors = YES;
    show_stats = NO;
    auto screen = [NSScreen mainScreen];
    NSRect screenrect;
    if(screen){
        screenrect = screen.visibleFrame;
    }
    else{
        screenrect = NSMakeRect(0, 0, 1400, 800);
    }
    auto split_view = [[NSSplitView alloc] initWithFrame:screenrect];
    split_view.vertical = YES;
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:(id)EDITOR_FONT, NSFontAttributeName, nil];
    auto Msize = [[NSAttributedString alloc] initWithString:@"M" attributes:attributes].size.width;
    auto textwidth  = 84*Msize;
    NSRect textrect = {.origin={screenrect.size.width-textwidth,0}, .size={textwidth,screenrect.size.height}};
    text = [[DndTextView alloc] initWithFrame:textrect font:EDITOR_FONT];
    highlighter = [[DndHighlighter alloc] init];
    highlighter.controller = self;
    text.textStorage.delegate = highlighter;
    text.minSize = NSMakeSize(0.0, textrect.size.height);
    text.maxSize = NSMakeSize(1e9, 1e9);
    text.verticallyResizable = YES;
    text.horizontallyResizable = NO;
    text.textContainer.containerSize = NSMakeSize(textrect.size.width, 1e9);
    text.textContainer.widthTracksTextView = YES;
    text.textContainerInset = NSMakeSize(4,4);

    editor_container = [[NSSplitView alloc] initWithFrame:textrect];
    editor_container.vertical = NO;

    error_text = [[NSTextView alloc] init];
    error_text.textStorage.font = EDITOR_FONT;
    error_text.editable = NO;
    error_text.usesAdaptiveColorMappingForDarkAppearance = YES;

    scrollview = [[NSScrollView alloc] initWithFrame:textrect];
    // scrollview.borderType = NSNoBorder;
    scrollview.hasVerticalScroller = YES;
    scrollview.hasHorizontalScroller = NO;
    scrollview.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin;
    scrollview.documentView = text;
    scrollview.findBarPosition = NSScrollViewFindBarPositionAboveContent;

    text.usesFindBar = YES;
    text.incrementalSearchingEnabled = YES;


    NSRect webrect = {.origin={0, 0}, .size={screenrect.size.width-textwidth, screenrect.size.height}};
    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    handler = [[DndUrlHandler alloc] init];
    handler.controller = self;
    [config.preferences setValue:@YES forKey:@"developerExtrasEnabled"];
    [config setURLSchemeHandler:handler forURLScheme:@"dnd"];
    webview = [[WKWebView alloc] initWithFrame:webrect configuration:config];
    webview.allowsMagnification = YES;
    webnavdel = [[WebNavDel alloc] init];
    webnavdel.controller = self;
    webview.navigationDelegate = webnavdel;
    [split_view addSubview:webview];
    [editor_container addSubview:scrollview];

    auto button_view = [[NSStackView alloc] init];
    {
        NSString* button_labels[] = {
            DND_AUTO_APPLY_CHANGES_LABEL,
            DND_READ_ONLY_LABEL,
            DND_COORD_HELPER_LABEL,
            DND_SHOW_ERRORS_LABEL,
            DND_SHOW_STATS_LABEL,
        };
        BOOL button_states[] = {
            auto_recalc,
            !text.editable,
            coord_helper,
            show_errors,
            show_stats,
        };
        _Static_assert(arrlen(button_states)==arrlen(button_labels), "");
        for(size_t i = 0; i < arrlen(button_labels); i++){
            auto button = [NSButton checkboxWithTitle:button_labels[i] target:self action:@selector(button_click:)];
            button.state = button_states[i]?NSControlStateValueOn:NSControlStateValueOff;
            [button_view addView:button inGravity:NSStackViewGravityLeading];
        }
    }
    [editor_container addSubview:error_text];
    [editor_container addSubview:button_view];
    [split_view addSubview:editor_container];
    webview.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    webview.allowsBackForwardNavigationGestures = YES;
    webview.UIDelegate = self;
    [split_view adjustSubviews];
    self.view = split_view;
    return self;
}
-(void)insert_file:(id)sender{
    NSMenuItem* item = sender;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    switch(item.tag){
        case GDND_INSERT_IMGLINKS:
        case GDND_INSERT_IMG:
            panel.allowedFileTypes = @[@"png"];
            break;
        case GDND_INSERT_CSS:
            panel.allowedFileTypes = @[@"css"];
            break;
        case GDND_INSERT_SCRIPT:
            panel.allowedFileTypes = @[@"js"];
            break;
        case GDND_INSERT_DND:
            panel.allowedFileTypes = @[@"dnd"];
            break;
    }
    [panel beginWithCompletionHandler:^(NSInteger result){
        if(result == NSModalResponseOK){
            assert(panel.URL);
            NSURL* url = panel.URL;
            NSSize size = {};
            if(item.tag == GDND_INSERT_IMGLINKS){
                NSImage* img = [ [NSImage alloc] initByReferencingURL:url];
                size.height = [img representations][0].pixelsHigh;
                size.width = [img representations][0].pixelsWide;
            }
            NSString* path;
            path = panel.URL.path;
            if(self->file_url){
                auto chosen_components = panel.URL.pathComponents;
                auto doc_components = self->file_url.pathComponents;
                if(doc_components.count > chosen_components.count){
                    goto have_path;
                }
                auto shorter = doc_components.count;
                NSUInteger i;
                for(i = 0; i < shorter; i++){
                    if([chosen_components[i] isEqual:doc_components[i]])
                        continue;
                    break;
                }
                if(i < shorter - 1){
                    goto have_path;
                }
                NSArray<NSString*>* chosen = [chosen_components subarrayWithRange:NSMakeRange(i, chosen_components.count-i)];
                path = [chosen componentsJoinedByString:@"/"];
            }

            have_path:;
            [self->text insert_file_block:path tag:item.tag size:size];
            [self recalc_html: [self get_text]];
        }
    }];
}
-(void)zoom_out:(id)sender{
    webview.magnification/=1.2;
}
-(void)zoom_in:(id)sender{
    webview.magnification*=1.2;
}
-(void)zoom_normal:(id)sender{
    webview.magnification = 1.0;
}
- (void)viewDidLoad {
    [super viewDidLoad];
}

-(void)format_dnd:(id)sender {
    // FIXME: restoring the scroll position doesn't work
    auto before = [self->scrollview lineScroll];
    NSString *string = self->text.string;
    const char* source_text = [string UTF8String];
    // auto t1 = get_t();
    LongString html = {};
    auto len = strlen(source_text);
    error_text.editable = YES;
    [[error_text textStorage].mutableString setString:@""];
    auto err = dndc_format((LongString){len, source_text}, &html, gdndc_error_func, (__bridge void*)error_text);
    error_text.editable = NO;
    if(err){
        return;
    }
    PushDiagnostic();
    SuppressCastQual();
    NSData* htmldata = [NSData dataWithBytesNoCopy:(void*)html.text length:html.length freeWhenDone:YES];
    PopDiagnostic();
    auto str = [[NSString alloc] initWithData:htmldata encoding:NSUTF8StringEncoding];
    if(!str)
        return;
    [self->text insertText:str replacementRange:NSMakeRange(0, [[self->text textStorage] length])];
    [self->scrollview setLineScroll:before];
}
-(void)save_scroll_position {
    [webview
        evaluateJavaScript:@ JSRAW(
            (function(){
                const result = {};
                const html = document.getElementsByTagName("html")[0];
                if(html.scrollLeft || html.scrollTop)
                    result.html = [html.scrollLeft, html.scrollTop];
                function get_scroll(ident){
                    let thing = document.getElementById(ident);
                    if(!thing){
                        let things = document.getElementsByClassName(ident);
                        if(things.length)
                            thing = things[0];
                    }
                    if(thing && (thing.scrollLeft || thing.scrollTop)){
                        result[ident] = [thing.scrollLeft, thing.scrollTop];
                    }
                }
                get_scroll("left");
                get_scroll("center");
                get_scroll("right");
                if(Object.keys(result).length){
                    return JSON.stringify(result);
                }
                return null;
            }());)
        completionHandler:^(id object, NSError*error){
            if(error){
                LOGIT(error);
                return;
            }
            if(object == [NSNull null])
                return;
            scroll_resto_string = object;
        }];
}
-(LongString)get_text{
    [self save_scroll_position];
    NSString *string = self->text.string;
    // Inject javascript that will restore the scroll position in the window.
    string = [string stringByAppendingString:
        @"\n"
        "::script @inline\n"
    ];
    string = [string stringByAppendingString:
        @"  "
        JSRAW(document.addEventListener("DOMContentLoaded", function(){
                  let request = new XMLHttpRequest();
                  request.open("GET", "dnd:///scrollresto", true);
                  request.onload = function(){
                    if(!request.response)
                        return;
                    const SCROLLRESTO = JSON.parse(request.response);
                    for(let [key, value] of Object.entries(SCROLLRESTO)){
                        if(key == "html"){
                            const html = document.getElementsByTagName("html")[0];
                            html.scrollLeft = value[0];
                            html.scrollTop = value[1];
                        }
                        else {
                            let thing = document.getElementById(key);
                            if(!thing){
                                let things = document.getElementsByClassName(key);
                                if(things.length)
                                    thing = things[0];
                            }
                            if(thing){
                                thing.scrollLeft = value[0];
                                thing.scrollTop = value[1];
                            }
                        }
                    }
                  };
                  request.send();
                });) "\n"];
    if(coord_helper){
        string = [string stringByAppendingString:
            @"\n"
            "::script @inline\n  "
            JSRAW(
            document.addEventListener("DOMContentLoaded", function(){
              const svgs = document.getElementsByTagName("svg");
              for(let i = 0; i < svgs.length; i++){
                const svg = svgs[i];
                const texts = svg.getElementsByTagName("text");
                let text_height = 0;
                if(texts.length){
                  const first_text = texts[0];
                  text_height = first_text.getBBox().height || 0;
                }
                svg.addEventListener("click", function(e){
                let name = prompt('Enter Room Name');
                if(name){
                  const x_scale = svg.width.baseVal.value / svg.viewBox.baseVal.width;
                  const y_scale = svg.height.baseVal.value / svg.viewBox.baseVal.height;
                  const rect = e.currentTarget.getBoundingClientRect();
                  const true_x = ((e.clientX - rect.x)/ x_scale) | 0;
                  const true_y = (((e.clientY - rect.y)/ y_scale) + text_height/2) | 0;
                  let request = new XMLHttpRequest();
                  if(!name.includes('.')){
                    name += '.';
                    }
                  const combined = '\n'+name+':'+":md .room @coord("+true_x+','+true_y+")\n";
                  request.open("POST", "dnd:///roomclick", true);
                  request.send(combined);
                  }
                });
              }
            });
        )];
    }
    const char* source_text = [string UTF8String];
    LongString source = {
        .text = source_text,
        // this is so dumb. Is there an API to get the length of the utf-8 string?
        // Maybe I should be turning NSString into
        // NSData and then borrowing the buffer?
        .length = strlen(source_text),
    };
    return source;
}
-(void)recalc_html:(LongString)source{
    // FIXME: don't do this synchronously
    // FIXME: where the fuck are you supposed to put this stuff.
    if(!auto_recalc)
        return;
    LongString html = {};
    NSString* final = [[self->file_url path] lastPathComponent];
    LongString outputpath;
    outputpath.text = [final UTF8String];
    outputpath.length = strlen(outputpath.text);
    LongString base_dir = LS("this.html");
    // auto t0 = get_t();
    uint64_t flags = 0;
    flags |= DNDC_PYTHON_IS_INIT;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    if(show_stats)
        flags |= DNDC_PRINT_STATS;
    // Disabled until I can figure out how to get wkwebview to invalidate
    // cached images.
    // flags |= DNDC_USE_DND_URL_SCHEME;
    error_text.editable = YES;
    [[error_text textStorage].mutableString setString:@""];
    auto err = run_the_dndc(flags, base_dir, source, outputpath, &html, BASE64CACHE, TEXTCACHE, show_errors?gdndc_error_func:NULL, show_errors?(__bridge void*)error_text:NULL, cache_watch_files, NULL, gdndc_ast_func, (__bridge void*)self).errored;
    // auto err = dndc_compile_dnd_file(flags, base_dir, source, &html, BASE64CACHE, TEXTCACHE, show_errors?gdndc_error_func:NULL, show_errors?(__bridge void*)error_text:NULL, cache_watch_files, NULL);
    error_text.editable = NO;
    // auto t1 = get_t();
    // HERE("dndc_compile_dnd_file: %.3fms", (t1-t0)/1000.);
    if(err){
        return;
    }
    PushDiagnostic();
    SuppressCastQual();
    NSData* htmldata = [NSData dataWithBytesNoCopy:(void*)html.text length:html.length+1 freeWhenDone:YES];
    PopDiagnostic();
    NSURL* url = [NSURL URLWithString:DND_THIS_URL];
    [webview loadData:htmldata MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:url];
    // auto t2 = get_t();
    // HERE("load the page: %.3fms", (t2-t1)/1000.);
}

-(void)loadView {
    auto screen = [NSScreen mainScreen];
    NSRect screenrect;
    if(screen){
        screenrect = screen.visibleFrame;
    }
    else{
        screenrect = NSMakeRect(0, 0, 1400, 800);
    }
    self.view = [[NSView alloc] initWithFrame: screenrect];
}
-(void)flop_editor:(id _Nullable)sender{
    if(scrollview.hidden)
        return;
    if(editor_on_left){
        [(NSSplitView*)self.view removeArrangedSubview:scrollview];
        [(NSSplitView*)self.view removeArrangedSubview:webview];
        [self.view addSubview:webview];
        [self.view addSubview:scrollview];
    }
    else {
        [(NSSplitView*)self.view removeArrangedSubview:scrollview];
        [(NSSplitView*)self.view removeArrangedSubview:webview];
        [self.view addSubview:scrollview];
        [self.view addSubview:webview];
    }
    editor_on_left = not editor_on_left;
    return;
}
-(void) toggle_editor:(id)sender{
    editor_container.hidden = !self->editor_container.hidden;
    if(editor_container.hidden){
        webview.frame = NSMakeRect(webview.frame.origin.x, webview.frame.origin.y, webview.frame.size.width + scrollview.frame.size.width, webview.frame.size.height);
    }
    else{
        webview.frame = NSMakeRect(webview.frame.origin.x, webview.frame.origin.y, webview.frame.size.width - scrollview.frame.size.width, webview.frame.size.height);
    }
}

- (void)webView:(WKWebView *)webView
runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt
    defaultText:(NSString *_Nullable)defaultText
initiatedByFrame:(WKFrameInfo *)frame
completionHandler:(void (^)(NSString *result))completionHandler{
    auto alert = [[NSAlert alloc] init];
    // [alert setTitle:@"Room"];
    alert.messageText = prompt;
    [alert addButtonWithTitle:@"Submit"];
    [alert addButtonWithTitle:@"Cancel"];
    alert.icon = nil;
    auto input_frame = NSMakeRect(0, 0, 300, 24);
    auto text_field = [[NSTextField alloc] initWithFrame:input_frame];
    alert.accessoryView = text_field;
    [[alert window] setInitialFirstResponder:text_field];
    auto response = [alert runModal];
    if(response == NSAlertFirstButtonReturn){
        completionHandler([text_field stringValue]);
    }
    else {
        completionHandler(@"");
    }
}

@end

@implementation DndWindowController : NSWindowController
-(NSString*)windowTitleForDocumentDisplayName:(NSString*)displayName{
    auto result = [self.document doc_title];
    if(!result)
        return displayName;
    return result;
}
// !!! this doesn't go here !!!
-(void)keyDown:(NSEvent*) event{
    if(event.modifierFlags & NSEventModifierFlagCommand){
        NSInteger num = [event.characters integerValue];
        if(num){
            num -= 1;
           auto tabs =  [NSApp mainWindow].tabbedWindows;
           if(tabs.count > num){
               [tabs[num] makeKeyAndOrderFront:nil];
               return;
           }
        }
    }
    [super keyDown:event];
}
@end

@implementation DndAppDelegate{
    DndFontDelegate* fontdel;
}
-(void)applicationWillFinishLaunching:(NSNotification *)notification{
    do_menus();
}
- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender{
    auto controller = [NSDocumentController sharedDocumentController];
    // Dude, there's no way this is how you are supposed to do this.
    // What was I thinking?
    [controller openDocument:nil];
    return NO;
}


-(void)flop_editors:(id)sender{
    auto windows = [NSApp windows];
    for(NSWindow* win in windows){
        NSViewController* vc = win.contentViewController;
        if([vc isKindOfClass:[DndViewController class]]){
         [(DndViewController*)vc flop_editor:nil];
        }
    }
}
-(void)purge_file_caches:(id)sender{
    dndc_filecache_clear(BASE64CACHE);
    dndc_filecache_clear(TEXTCACHE);
}

-(void)change_font{
    auto mgr = [NSFontManager sharedFontManager];
    LOGIT([mgr selectedFont]);
}
-(void)applicationDidFinishLaunching:(NSNotification *)notification{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSApp.applicationIconImage = appimage;
    auto panel = [NSFontPanel sharedFontPanel];
    fontdel = [[DndFontDelegate alloc] init];
    panel.delegate = fontdel;

    [panel setPanelFont:EDITOR_FONT isMultiple:NO];
    auto mgr = [NSFontManager sharedFontManager];
    [mgr setTarget:fontdel];
    // [mgr setAction:@selector(change_font)];
}

@end

@implementation DndFontDelegate
// this shit is deprecated apparently.
// but uh. Whatever.
-(void)changeFont:(nullable id)sender{
    auto font = [sender convertFont:EDITOR_FONT];
    if(!font)
        return;
    EDITOR_FONT = font;
    auto controller = [NSDocumentController sharedDocumentController];
    auto documents = [controller documents];
    for(DndDocument* doc in documents){
        [doc change_font:EDITOR_FONT];
    }
}
@end



extern char _app_icon[];
extern char _app_icon_end[];
asm(".global __app_icon\n"
    ".global __app_icon_end\n"
    "__app_icon:\n"
#ifdef APP_ICON_PATH
    ".incbin \"" APP_ICON_PATH "\"\n"
#else
    ".incbin \"Platform/MacOS/app_icon.png\"\n"
#endif
    "__app_icon_end:\n");

int
main(int argc, const char *_Null_unspecified *_Nonnull argv) {
    if(dndc_init_python() != 0)
        return 1;
    auto font = [NSFont fontWithName:@"SF Mono" size:11];
    if(!font)
        font = [NSFont fontWithName:@"Courier" size:11];
    if(!font)
        font = [NSFont fontWithName:@"Menlo" size:11];
    EDITOR_FONT = font;
    BASE64CACHE = dndc_create_filecache();
    TEXTCACHE = dndc_create_filecache();
    NSApplication* app = [NSApplication sharedApplication];
    DndAppDelegate* appDelegate = [DndAppDelegate new];
    app.delegate = appDelegate;
    auto icon_size = _app_icon_end - _app_icon;
    NSData* imagedata = [NSData dataWithBytesNoCopy:(void*)_app_icon length:icon_size freeWhenDone:NO];
    PushDiagnostic();
    #pragma clang diagnostic ignored "-Wnullable-to-nonnull-conversion"
    appimage = [[NSImage alloc] initWithData:imagedata];
    PopDiagnostic();
    app.applicationIconImage = appimage;
    SYNTAX_COLORS[DNDC_SYNTAX_DOUBLE_COLON]       = [NSColor lightGrayColor];
    SYNTAX_COLORS[DNDC_SYNTAX_HEADER]             = [NSColor systemBlueColor];
    SYNTAX_COLORS[DNDC_SYNTAX_NODE_TYPE]          = [NSColor darkGrayColor];
    SYNTAX_COLORS[DNDC_SYNTAX_ATTRIBUTE]          = [NSColor systemBrownColor];
    SYNTAX_COLORS[DNDC_SYNTAX_ATTRIBUTE_ARGUMENT] = [NSColor systemBrownColor];
    SYNTAX_COLORS[DNDC_SYNTAX_CLASS]              = [NSColor systemGrayColor];
    // SYNTAX_COLORS[DNDC_SYNTAX_BULLET]          =
    // SYNTAX_COLORS[DNDC_SYNTAX_COMMENT]         =
    SYNTAX_COLORS[DNDC_SYNTAX_RAW_STRING]         = [NSColor greenColor]; // currently unused
    return NSApplicationMain(argc, argv);
}

static
void
do_menus(void){
    NSMenu *mainMenu = [[NSMenu alloc] init];
    // Create the main menu bar
    [NSApp setMainMenu:mainMenu];

    {
        NSString *appName = @"Gdndc";
        // Create the application menu
        NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

        // Add menu items
        NSString *title = [@"About " stringByAppendingString:appName];
        [menu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

        [menu addItem:[NSMenuItem separatorItem]];

        [menu addItemWithTitle:@"Preferences…" action:nil keyEquivalent:@","];

        [menu addItem:[NSMenuItem separatorItem]];

        NSMenu* serviceMenu = [[NSMenu alloc] initWithTitle:@""];
        NSMenuItem* menu_item = [menu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:serviceMenu];

        [NSApp setServicesMenu:serviceMenu];

        [menu addItem:[NSMenuItem separatorItem]];

        title = [@"Hide " stringByAppendingString:appName];
        [menu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

        menu_item = [menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [menu_item setKeyEquivalentModifierMask:(NSEventModifierFlagOption|NSEventModifierFlagCommand)];

        [menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

        [menu addItem:[NSMenuItem separatorItem]];

        title = [@"Quit " stringByAppendingString:appName];
        [menu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

        menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }

    // Create the File menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"File"];
        [menu addItemWithTitle:@"New Tab" action:@selector(newWindowForTab:) keyEquivalent:@"n"];
        [menu addItemWithTitle:@"New Tab" action:@selector(newWindowForTab:) keyEquivalent:@"t"];
        [menu addItemWithTitle:@"Open" action:@selector(openDocument:) keyEquivalent:@"o"];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
        [menu addItemWithTitle:@"Save" action:@selector(saveDocument:) keyEquivalent:@"s"];
        [menu addItemWithTitle:@"Revert to Saved" action:@selector(revertDocumentToSaved:) keyEquivalent:@""];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Empty File Caches" action:@selector(purge_file_caches:) keyEquivalent:@""];
        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }
    // Create the edit menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Edit"];
        [menu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
        [menu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
        NSMenuItem* fontmi = [[NSMenuItem alloc] initWithTitle:@"Font" action:nil keyEquivalent:@""];
        NSFontManager *fontManager = [NSFontManager sharedFontManager];
        NSMenu *fontMenu = [fontManager fontMenu:YES];
        [fontmi setSubmenu:fontMenu];
        [menu addItem:fontmi];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
        [menu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
        [menu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
        [menu addItemWithTitle:@"Indent" action:@selector(indent:) keyEquivalent:@">"];
        [menu addItemWithTitle:@"Dedent" action:@selector(dedent:) keyEquivalent:@"<"];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
        [menu addItemWithTitle:@"Format" action:@selector(format_dnd:) keyEquivalent:@"J"];
        NSMenuItem* mi = [[NSMenuItem alloc] initWithTitle:@"Find..." action:@selector(performTextFinderAction:) keyEquivalent:@"f"];
        mi.tag = NSTextFinderActionShowFindInterface;
        [menu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Find and Replace..." action:@selector(performTextFinderAction:) keyEquivalent:@"F"];
        mi.tag = NSTextFinderActionShowReplaceInterface;
        [menu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Find Next" action:@selector(performTextFinderAction:) keyEquivalent:@"g"];
        mi.tag = NSTextFinderActionNextMatch;
        [menu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Find Previous" action:@selector(performTextFinderAction:) keyEquivalent:@"G"];
        mi.tag = NSTextFinderActionPreviousMatch;
        [menu addItem:mi];

        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }
    // Create the insert menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Insert"];
        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];

        NSMenuItem* mi;
        mi = [[NSMenuItem alloc] initWithTitle:@"Image" action:@selector(insert_file:) keyEquivalent:@"i"];
        mi.tag = GDND_INSERT_IMG;
        [menu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Imglinks" action:@selector(insert_file:) keyEquivalent:@"I"];
        mi.tag = GDND_INSERT_IMGLINKS;
        [menu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"CSS" action:@selector(insert_file:) keyEquivalent:@""];
        mi.tag = GDND_INSERT_CSS;
        [menu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Script" action:@selector(insert_file:) keyEquivalent:@""];
        mi.tag = GDND_INSERT_SCRIPT;
        [menu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Import" action:@selector(insert_file:) keyEquivalent:@""];
        mi.tag = GDND_INSERT_DND;
        [menu addItem:mi];

        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }
    // Create the view menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"View"];
        [menu addItemWithTitle:@"Toggle Editor" action:@selector(toggle_editor:) keyEquivalent:@"j"];
        [menu addItemWithTitle:@"Flop Editor" action:@selector(flop_editors:) keyEquivalent:@""];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Refresh" action:@selector(refresh) keyEquivalent:@"r"];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Zoom Out" action:@selector(zoom_out:) keyEquivalent:@"-"];
        [menu addItemWithTitle:@"Zoom In" action:@selector(zoom_in:) keyEquivalent:@"+"];
        [menu addItemWithTitle:@"Actual Size" action:@selector(zoom_normal:) keyEquivalent:@"0"];


        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }


    // Create the window menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Window"];

        [menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
        [menu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];

        [NSApp setWindowsMenu:menu];
    }
    // Create the help menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Help"];

        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
        [NSApp setHelpMenu:menu];
    }
}

#pragma clang assume_nonnull end

#import "dndc.c"
#import "allocator.c"
