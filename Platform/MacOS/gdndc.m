#import <Cocoa/Cocoa.h>
#import <Webkit/WebKit.h>
#include "common_macros.h"
#include "measure_time.h"
#include "dndc.h"
// I need to build strings!
#include "MStringBuilder.h"
#include "mallocator.h"
#include "msb_format.h"

#if !__has_feature(objc_arc)
#error "ARC is off"
#endif

static struct DndcFileCache*_Nonnull BASE64CACHE;
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
    GDND_INSERT_JS,
    GDND_INSERT_CSS,
    GDND_INSERT_DND,
}GdndInsertTag;
@interface DndWindowController: NSWindowController
// Has a NSWindow* window
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
@public DndHighlighter* highlighter; // for the text
@public WKWebView* webview;
@public WebNavDel* webnavdel; // for the webview
@public NSURL* file_url;
@public DndUrlHandler* handler;
}
-(LongString)get_text;
-(void)recalc_html:(LongString)text;
-(void)flop_editor:(id)sender;
@end

//
// The Dnd document
//
@interface DndDocument: NSDocument{
// this is kind of janky, but whatever
DndViewController* view_controller;
}
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
-(instancetype)init {
    self = [super init];
    self->view_controller = [[DndViewController alloc] init];
    return self;
}
- (instancetype)initForURL:(NSURL *)urlOrNil
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
    auto mainwindow = [NSApp mainWindow];
    NSWindow* docwindow = [self make_window];
    DndWindowController* winc = [[DndWindowController alloc] initWithWindow:docwindow];
    // is there a method for this or are you supposed to assign them like this?
    [self addWindowController:winc];
    if(mainwindow){
        [mainwindow addTabbedWindow:docwindow ordered:NSWindowAbove];
    }
    [docwindow makeKeyAndOrderFront: nil];
}
-(NSString*)wv_title{
    return self->view_controller->webview.title;
}

- (NSData*)dataOfType:(NSString *)typeName error:(NSError **)outError {
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

static NSColor* SYNTAX_COLORS[DNDC_SYNTAX_MAX] = {};
#define U16SYNTAX
#ifdef U16SYNTAX
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
#else
struct SyntaxData {
    NSTextStorage* storage;
    const char* begin;
    const char* begin_edited_line;
    const char* end_edited_lines;
};
static
void
dndc_syntax_func(void* _Nullable data, int type, int line, int col, Nonnull(const char*)begin, size_t length){
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
#endif

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
#if 1
    // NSRange currentLineRange = NSMakeRange(0, [string length]);
    NSRange currentLineRange = [string lineRangeForRange:editedRange];
    // auto before= get_t();
    [textStorage removeAttribute:NSForegroundColorAttributeName range:currentLineRange];
    [textStorage removeAttribute:NSBackgroundColorAttributeName range:currentLineRange];
    // HERE("Clearing syntax costs: %.3fms", (get_t()-before)/1000.);
#ifdef U16SYNTAX
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
#else
    struct SyntaxData sd = {
        .storage = textStorage,
        .begin = text.text,
        .begin_edited_line = currentLineRange.location + text.text,
        .end_edited_lines = currentLineRange.location+currentLineRange.length+text.text,
    };
    // auto t0 = get_t();
    // for(int i = 0; i < 1000; i++)
        dndc_analyze_syntax(LS_to_SV(text), dndc_syntax_func, &sd);
    auto t1 = get_t();
#endif
    // HERE("dndc_analyze_syntax: %.3fms", (t1-t0)/1000.);
    return;
#else
    // We take advantage of the fact that .dnd can mostly be tokenized
    // linewise (technically you need to know what the parent node is, but
    // that only affects python blocks really).
    // NSRange currentLineRange = NSMakeRange(0, [string length]);
    NSRange currentLineRange = [string lineRangeForRange:editedRange];
    [textStorage removeAttribute:NSForegroundColorAttributeName range:currentLineRange];
    [textStorage removeAttribute:NSBackgroundColorAttributeName range:currentLineRange];
    NSUInteger rowstart = currentLineRange.location;
    BOOL saw_colon = NO;
    BOOL saw_double_colon = NO;
    BOOL all_spaces = YES;
    NSUInteger double_colon = 0;
    auto end = currentLineRange.location + currentLineRange.length;
    for (NSUInteger i = currentLineRange.location; i < end; i++){
        unichar c = [string characterAtIndex:i];
        if(all_spaces && c != ' '){
            all_spaces = NO;
        }
        if(c == '\n'){
            if(saw_double_colon){
                if(rowstart < double_colon){
                    [textStorage addAttribute:NSForegroundColorAttributeName value:[NSColor blueColor] range:NSMakeRange(rowstart, double_colon-rowstart)];
                }
                [textStorage addAttribute:NSForegroundColorAttributeName value:[NSColor lightGrayColor] range:NSMakeRange(double_colon, i-double_colon)];
                saw_double_colon = NO;
                saw_colon = NO;
            }
            all_spaces = YES;
            rowstart = i+1;
            continue;
        }
        if(c == ':'){
            if(saw_double_colon)
                continue;
            if(saw_colon){
                double_colon = i-1;
                saw_double_colon = YES;
                continue;
            }
            saw_colon = YES;
            continue;
        }
        if(c == ' ' && all_spaces){
            saw_colon = NO;
            continue;
        }
        if(c == '*' && all_spaces){
            [textStorage addAttribute:NSForegroundColorAttributeName value:[NSColor grayColor] range:NSMakeRange(i, 1)];
            all_spaces = NO;
            saw_colon = NO;
            continue;
        }
        all_spaces = NO;
        saw_colon = NO;
    }
    if(saw_double_colon){
        [textStorage addAttribute:NSForegroundColorAttributeName value:[NSColor blueColor] range:NSMakeRange(rowstart, double_colon-rowstart)];
        [textStorage addAttribute:NSForegroundColorAttributeName value:[NSColor lightGrayColor] range:NSMakeRange(double_colon, end-double_colon)];
        saw_double_colon = NO;
        saw_colon = NO;
    }
    auto t1 = get_t();
    HERE("dndc_analyze_syntax: %.3fms", (t1-t0)/1000.);
#endif
}
@end

@implementation DndTextView
-(void)indent:(id)sender{
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
-(void)dedent:(id)sender{
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
    // TODO: maybe this should be a getter instead?
    if(!indent_pattern)
        indent_pattern = [[NSRegularExpression alloc] initWithPattern:kIndentPatternString options:0 error:nil];
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
        case GDND_INSERT_JS:
            [self insert_block:path at:r indent_amount:rel name:SV("::js\n")];
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
    msb_reserve(&sb, 256);
    msb_write_str(&sb, blockname.text, blockname.length);
    msb_write_nchar(&sb, ' ', indent_amount+2);
    const char* cpath = [path UTF8String];
    msb_write_str(&sb, cpath, strlen(cpath));
    msb_write_char(&sb, '\n');
    auto strdata = msb_detach(&sb);
    PushDiagnostic();
    SuppressCastQual();
    NSData* data = [NSData dataWithBytesNoCopy:(void*)strdata.text length:strdata.length freeWhenDone:YES];
    PopDiagnostic();
    auto to_insert = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    [self insertText:to_insert replacementRange:r];
}
-(void)insert_imglinks_block:(NSString*)path at:(NSRange)r indent_amount:(NSInteger)indent_amount size:(NSSize)size{
    MStringBuilder sb = {.allocator = get_mallocator()};
    msb_reserve(&sb, 256);
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
    auto strdata = msb_detach(&sb);
    PushDiagnostic();
    SuppressCastQual();
    NSData* data = [NSData dataWithBytesNoCopy:(void*)strdata.text length:strdata.length freeWhenDone:YES];
    PopDiagnostic();
    auto to_insert = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    [self insertText:to_insert replacementRange:r];
}
+(NSMenu*)defaultMenu{
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
    item = [[NSMenuItem alloc] initWithTitle:@"Insert JS" action:@selector(insert_file:) keyEquivalent:@""];
    item.tag = GDND_INSERT_JS;

    [result addItem:item];
    item = [[NSMenuItem alloc] initWithTitle:@"Insert DND" action:@selector(insert_file:) keyEquivalent:@""];
    item.tag = GDND_INSERT_DND;

    [result addItem:item];
    [result addItem:[NSMenuItem separatorItem]];
    return result;
}
-(NSString *)preferredPasteboardTypeFromArray:(NSArray *)availableTypes restrictedToTypesFromArray:(NSArray *)allowedTypes {
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

-(void)deleteBackward:(id)sender{
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
-(void)insertNewline:(id)sender {
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
- (void)insertTab:(id)sender{
 [self insertText: @"  " replacementRange:self.selectedRange];
}

@end

@implementation WebNavDel: NSObject
-(void)webView:(WKWebView *)webView
    didFinishNavigation:(WKNavigation *)navigation{
    auto title = webView.title;
    // Hack
    [NSApp mainWindow].title = title;
}
-(void)webView:(WKWebView *)webView
    decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler{
        auto path = navigationAction.request.URL.relativePath;
        if([path isEqual:@"/this.html"]){
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }
        if([path characterAtIndex:0] == '/'){
            // auto real_url = [[[NSURL fileURLWithPath:[path substringFromIndex:1]] URLByDeletingPathExtension] URLByAppendingPathExtension:@"dnd"];
            // auto foo = self->controller->file_url.URLByDeletingLastPathComponent;
            auto real_url = [self.controller->file_url.URLByDeletingLastPathComponent URLByAppendingPathComponent:[path substringFromIndex:1]];
            real_url = [[real_url URLByDeletingPathExtension] URLByAppendingPathExtension:@"dnd"];
            // hack: too lazy to declare interfaces
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
    auto response = [[NSURLResponse alloc]
        initWithURL:(NSURL*)[NSURL URLWithString:@"dnd://./this.html"]
        MIMEType:@"text/plain"
        expectedContentLength:0
        textEncodingName:nil];
    [urlSchemeTask didReceiveResponse:response];
    auto request = [urlSchemeTask request];
    NSURL* url = request.URL;
    auto method = [request HTTPMethod];
    if([method isEqualToString:@"POST"] and [url isEqual:[NSURL URLWithString:@"dnd:///roomclick"]]){
        NSString* body = [[NSString alloc] initWithData:(NSData*)[request HTTPBody] encoding:NSUTF8StringEncoding];
        [urlSchemeTask didFinish];
        [self.controller->text insertText:body replacementRange:NSMakeRange([[self.controller->text textStorage] length], 0)];
        // HEREPrint([body UTF8String]);
    }
    else {
        [urlSchemeTask didFailWithError:[NSError errorWithDomain:@"denied" code:1 userInfo:nil]];
    }
}
-(void)webView:(WKWebView*)webView stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask{
}
@end


@implementation DndViewController {
BOOL editor_on_left;
BOOL auto_recalc;
BOOL coord_helper;
}
#define DND_AUTO_APPLY_CHANGES_LABEL @"Auto-apply changes"
#define DND_READ_ONLY_LABEL @"Read-only"
#define DND_COORD_HELPER_LABEL @"Coord helper"
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
    else {
        NSLog(@"Unknown button title:%@", title);
    }
}
-(instancetype)init{
    self = [super init];
    editor_on_left = NO;
    coord_helper = NO;
    auto_recalc = YES;
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
    auto font=[NSFont fontWithName:@"Menlo" size:11];
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:(id)font, NSFontAttributeName, nil];
    auto Msize = [[NSAttributedString alloc] initWithString:@"M" attributes:attributes].size.width;
    auto textwidth  = 84*Msize;
    NSRect textrect = {.origin={screenrect.size.width-textwidth,0}, .size={textwidth,screenrect.size.height}};
    text = [[DndTextView alloc] initWithFrame:textrect font:font];
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
    auto button = [NSButton checkboxWithTitle:DND_AUTO_APPLY_CHANGES_LABEL target:self action:@selector(button_click:)];
    button.state = NSControlStateValueOn;
    auto button_view = [[NSStackView alloc] init];
    [button_view addView:button inGravity:NSStackViewGravityLeading];
    button = [NSButton checkboxWithTitle:DND_READ_ONLY_LABEL target:self action:@selector(button_click:)];
    [button_view addView:button inGravity:NSStackViewGravityLeading];
    button = [NSButton checkboxWithTitle:DND_COORD_HELPER_LABEL target:self action:@selector(button_click:)];
    [button_view addView:button inGravity:NSStackViewGravityLeading];
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
        case GDND_INSERT_JS:
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
    auto err = dndc_format((LongString){len, source_text}, &html, dndc_stderr_error_func, NULL);
    // TODO: report?
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

-(LongString)get_text{
    NSString *string = self->text.string;
    if(coord_helper){
        string = [string stringByAppendingString:
            @"\n"
            "::js @inline\n"
            "  document.addEventListener('DOMContentLoaded', function(){\n"
            "    const svgs = document.getElementsByTagName('svg');\n"
            "    for(let i = 0; i < svgs.length; i++){\n"
            "      const svg = svgs[i];\n"
            "      const texts = svg.getElementsByTagName('text');\n"
            "      var text_height = 0;\n"
            "      if(texts.length){\n"
            "        const first_text = texts[0];\n"
            "        const text_height = first_text.getBBox().height || 0;\n"
            "      }\n"
            "      svg.addEventListener('click', function(e){\n"
            "      var name = prompt('Enter Room Name');\n"
            "      if(name){\n"
            "        const x_scale = svg.width.baseVal.value / svg.viewBox.baseVal.width;\n"
            "        const y_scale = svg.height.baseVal.value / svg.viewBox.baseVal.height;\n"
            "        const rect = e.currentTarget.getBoundingClientRect();\n"
            "        const true_x = ((e.clientX - rect.x)/ x_scale) | 0;\n"
            "        const true_y = (((e.clientY - rect.y)/ y_scale) + text_height/2) | 0;\n"
            "        let request = new XMLHttpRequest();\n"
            "        if(!name.includes('.')){\n"
            "          name += '.';\n"
            "          }\n"
            "        const combined = '\\n'+name+':'+':md .room @coord('+true_x+','+true_y+')\\n';\n"
            "        request.open('POST', 'dnd:///roomclick', true);\n"
            "        request.send(combined);\n"
            "        }\n"
            "      });\n"
            "    }\n"
            "  });\n"];
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
    NSString* dir = [[self->file_url URLByDeletingLastPathComponent] path];
    StringView base_dir;
    if(dir){
        // TODO: is there a more efficient way to
        // turn an NSString into a string view?
        const char* dir_text = [dir UTF8String];
        base_dir.text = dir_text;
        base_dir.length = strlen(dir_text);
    }
    else {
        base_dir = SV("");
    }
    // auto t0 = get_t();
    uint64_t flags = 0;
    flags |= DNDC_OUTPUT_IS_OUT_PARAM;
    flags |= DNDC_PYTHON_IS_INIT;
    flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    // flags |= DNDC_PRINT_STATS;
    auto err = dndc_compile_dnd_file(flags, base_dir, source, &html, (union DndcDependsArg){}, BASE64CACHE, NULL, dndc_stderr_error_func, NULL);
    // auto t1 = get_t();
    // HERE("dndc_compile_dnd_file: %.3fms", (t1-t0)/1000.);
    if(err){
        // TODO: report errors to the user (need to figure out the UX though).
        return;
    }
    PushDiagnostic();
    SuppressCastQual();
    NSData* htmldata = [NSData dataWithBytesNoCopy:(void*)html.text length:html.length+1 freeWhenDone:YES];
    PopDiagnostic();
    NSURL* url = [NSURL URLWithString:@"dnd://./this.html"];
    [webview loadData:htmldata MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:url];
    // auto t2 = get_t();
    // HERE("load the page: %.3fms", (t2-t1)/1000.);
}

-(void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];
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
-(void)flop_editor:(id)sender{
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
    defaultText:(NSString *)defaultText
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
    auto result = [self.document wv_title];
    if(!result)
        return displayName;
    return result;
}
// !!! this doesn't go here !!!
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
@end

@implementation DndAppDelegate : NSObject
-(void)applicationWillFinishLaunching:(NSNotification *)notification{
    do_menus();
}
- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender{
    auto controller = [NSDocumentController sharedDocumentController];
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
-(void)purge_img_cache:(id)sender{
    dndc_filecache_clear(BASE64CACHE);
}

-(void)applicationDidFinishLaunching:(NSNotification *)notification{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSApp.applicationIconImage = appimage;
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
main(int argc, const char * argv[]) {
    if(dndc_init_python() != 0)
        return 1;
    BASE64CACHE = dndc_create_filecache();
    NSApplication* app = [NSApplication sharedApplication];
    DndAppDelegate* appDelegate = [DndAppDelegate new];
    app.delegate = appDelegate;
    auto icon_size = _app_icon_end - _app_icon;
    NSData* imagedata = [NSData dataWithBytesNoCopy:(void*)_app_icon length:icon_size freeWhenDone:NO];
    appimage = [[NSImage alloc] initWithData:imagedata];
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
        [menu addItemWithTitle:@"Revert to Saved" action:@selector(revertDocumentToSaved:) keyEquivalent:@"r"];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Empty Image Cache" action:@selector(purge_img_cache:) keyEquivalent:@""];
        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }
    // Create the edit menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Edit"];
        [menu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
        [menu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
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

        mi = [[NSMenuItem alloc] initWithTitle:@"JS" action:@selector(insert_file:) keyEquivalent:@""];
        mi.tag = GDND_INSERT_JS;
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

#include "allocator.c"
#include "dndc.c"
