#import <Cocoa/Cocoa.h>
#import <Webkit/WebKit.h>
#include "measure_time.h"
#include "DndC/dndc.h"
// fuck it, I need to build strings!
#include "MStringBuilder.h"
#include "mallocator.h"

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

@interface DndTextView: NSTextView
-(void)insert_file_block:(NSString*)path tag:(GdndInsertTag)tag size:(NSSize)size;
@end

@interface DndHighlighter: NSObject<NSTextStorageDelegate>
@end

@interface WebNavDel : NSObject <WKNavigationDelegate>
@end

@interface DndViewController: NSViewController{
@public DndTextView* text;
@public NSScrollView* scrollview; // contains the text
@public DndHighlighter* highlighter; // for the text
@public WKWebView* webview;
@public WebNavDel* webnavdel; // for the webview
@public NSURL* file_url;
}
-(void)recalc_html:(id)sender;
@end

@interface DndDocument: NSDocument{
// this is kind of janky, but whatever
DndViewController* view_controller;

}
@end

@interface DndAppDelegate : NSObject<NSApplicationDelegate>
@end

static void do_menus(void);
static NSString * const kIndentPatternString = @"^(\\t|\\s)+";
static NSRegularExpression* indent_pattern;
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
-(NSWindow*)make_window{
    auto screen = [NSScreen mainScreen];
    NSRect rect = screen? screen.visibleFrame : NSMakeRect(0, 0, 1400, 800);

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect: rect
        styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
        backing: NSBackingStoreBuffered
        defer: NO];
    window.title = @"DndC";
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
        view_controller->text.string = str;
        view_controller->file_url = [self fileURL];
        [view_controller recalc_html:nil];
    }
    return YES;
}
@end

@implementation DndHighlighter
- (void)textStorage:(NSTextStorage *)textStorage
  didProcessEditing:(NSTextStorageEditActions)editedMask
              range:(NSRange)editedRange
     changeInLength:(NSInteger)delta{
    NSString *string = textStorage.string;
    NSRange currentLineRange = [string lineRangeForRange:editedRange];
    [textStorage removeAttribute:NSForegroundColorAttributeName range:currentLineRange];
    [textStorage removeAttribute:NSBackgroundColorAttributeName range:currentLineRange];
    NSUInteger rowstart = currentLineRange.location;
    BOOL saw_colon = NO;
    BOOL saw_double_colon = NO;
    BOOL all_spaces = YES;
    NSUInteger double_colon = 0;
    auto end = currentLineRange.location + currentLineRange.length;
    for (NSUInteger i = currentLineRange.location; i < end; i++) {
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
    auto thing = (DndViewController*)[NSApp keyWindow].contentViewController;
    if(thing){
        [thing recalc_html:nil];
    }
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
    for(size_t i = 0; i < indent_amount+2; i++){
        msb_write_char(&sb, ' ');
    }
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
#define DO_INDENT() do{\
        msb_reserve(&sb, indent_amount+2); \
        memset(sb.data+sb.cursor, ' ', indent_amount+2); \
        sb.cursor += indent_amount+2; \
    }while(0)

    DO_INDENT();
    const char* imgpath = [path UTF8String];
    msb_write_str(&sb, imgpath, strlen(imgpath));
    msb_write_char(&sb, '\n');
    double scale = Min(800.0/size.width, 800.0/size.height);
    DO_INDENT(); msb_sprintf(&sb, "width = %d\n", (int)(size.width*scale));
    DO_INDENT(); msb_sprintf(&sb, "height = %d\n", (int)(size.height*scale));
    DO_INDENT(); msb_sprintf(&sb, "viewBox = 0 0 %d %d\n", (int)size.width, (int)size.height);
    StringView script[] = {
        SV("::python\n"),
        SV("  # this is an example of how to script the imglinks\n"),
        SV("  imglinks = node.parent\n"),
        SV("  parent = imglinks.parent.parent\n"),
        SV("  for c in parent.children:\n"),
        SV("    if 'coord' in c.attributes:\n"),
        SV("      lead = c.header  # change this probably\n"),
        SV("      position = c.attributes['coord']\n"),
        SV("      imglinks.add_child(f'{lead} = {ctx.outfile}#{c.id} @{position}')\n"),
        SV("  #endpython\n"),
    };
    for(int i = 0; i < arrlen(script); i++){
        DO_INDENT();
        msb_write_str(&sb, script[i].text, script[i].length);
    }
#undef DO_INDENT
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
            auto real_url = [[[NSURL fileURLWithPath:[path substringFromIndex:1]] URLByDeletingPathExtension] URLByAppendingPathExtension:@"dnd"];
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


@implementation DndViewController
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
            [self recalc_html:nil];
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
-(instancetype)init{
    self = [super init];
    auto screen = [NSScreen mainScreen];
    NSRect screenrect;
    if(screen){
        screenrect = screen.visibleFrame;
    }
    else{
        screenrect = NSMakeRect(0, 0, 1400, 800);
    }
    auto font=[NSFont fontWithName:@"Menlo" size:11];
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:(id)font, NSFontAttributeName, nil];
    auto Msize = [[NSAttributedString alloc] initWithString:@"M" attributes:attributes].size.width;
    auto textwidth  = 84*Msize;
    NSRect textrect = {.origin={screenrect.size.width-textwidth,0}, .size={textwidth,screenrect.size.height}};
    text = [[DndTextView alloc] initWithFrame:textrect font:font];
    highlighter = [[DndHighlighter alloc] init];
    text.textStorage.delegate = highlighter;
    text.minSize = NSMakeSize(0.0, textrect.size.height);
    text.maxSize = NSMakeSize(1e9, 1e9);
    text.verticallyResizable = YES;
    text.horizontallyResizable = NO;
    text.textContainer.containerSize = NSMakeSize(textrect.size.width, 1e9);
    text.textContainer.widthTracksTextView = YES;
    text.textContainerInset = NSMakeSize(4,4);

    scrollview = [[NSScrollView alloc] initWithFrame:textrect];
    // scrollview.borderType = NSNoBorder;
    scrollview.hasVerticalScroller = YES;
    scrollview.hasHorizontalScroller = NO;
    scrollview.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin;
    scrollview.documentView = text;
    scrollview.findBarPosition = NSScrollViewFindBarPositionAboveContent;

    text.usesFindBar = YES;
    text.incrementalSearchingEnabled = YES;

    [self.view addSubview:scrollview];

    NSRect webrect = {.origin={0, 0}, .size={screenrect.size.width-textwidth, screenrect.size.height}};
    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    [config.preferences setValue:@YES forKey:@"developerExtrasEnabled"];
    webview = [[WKWebView alloc] initWithFrame:webrect configuration:config];
    webview.allowsMagnification = YES;
    webnavdel = [[WebNavDel alloc] init];
    webview.navigationDelegate = webnavdel;
    [self.view addSubview:webview];
    webview.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    webview.allowsBackForwardNavigationGestures = YES;
    [self recalc_html:nil];
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
}

-(void)format_dnd:(id)sender {
    auto before = [self->scrollview lineScroll];
    NSString *string = self->text.string;
    const char* source_text = [string UTF8String];
    // auto t1 = get_t();
    LongString html = {};
    auto len = strlen(source_text);
    auto err = dndc_format((LongString){len, source_text}, &html);
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

-(void)recalc_html:(id)sender {
    // FIXME: don't do this synchronously
    // FIXME: where the fuck are you supposed to put this stuff.
    // auto t0 = get_t();
    NSString *string = self->text.string;
    const char* source_text = [string UTF8String];
    // auto t1 = get_t();
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
    LongString source = {
        .text = source_text,
        // this is so dumb. Is there an API to get the length of the utf-8 string?
        // Maybe I should be turning NSString into
        // NSData and then borrowing the buffer?
        .length = strlen(source_text),
    };
    auto err = dndc_make_html(base_dir, source, &html);
    // auto t2 = get_t();
    if(err){
        // TODO: report errors to the user (need to figure out the UX though).
        return;
    }
    PushDiagnostic();
    SuppressCastQual();
    NSData* htmldata = [NSData dataWithBytesNoCopy:(void*)html.text length:html.length+1 freeWhenDone:YES];
    PopDiagnostic();
    // auto t4 = get_t();
    NSURL* url = [NSURL URLWithString:@"https://./this.html"];
    [webview loadData:htmldata MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:url];
    // auto t5 = get_t();
    // HERE("Getting string: %.3fms", (t1-t0)/1000.);
    // HERE("dndc: %.3fms", (t2-t1)/1000.);
    // HERE("Storing NSString: %.3fms", (t4-t2)/1000.);
    // HERE("Loading in webview: %.3fms", (t5-t4)/1000.);
    // HERE("Total: %.3fms", (t5-t0)/1000.);
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];
}

- (void)loadView {
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
-(void) toggle_editor:(id)sender{
    scrollview.hidden = !self->scrollview.hidden;
    if(scrollview.hidden){
        webview.frame = NSMakeRect(webview.frame.origin.x, webview.frame.origin.y, webview.frame.size.width + scrollview.frame.size.width, webview.frame.size.height);
    }
    else{
        webview.frame = NSMakeRect(webview.frame.origin.x, webview.frame.origin.y, webview.frame.size.width - scrollview.frame.size.width, webview.frame.size.height);
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


static void do_menus(void);

@implementation DndAppDelegate : NSObject
-(void)applicationWillFinishLaunching:(NSNotification *)notification{
    do_menus();
}
- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender{
    auto controller = [NSDocumentController sharedDocumentController];
    [controller openDocument:nil];
    return NO;
}



- (void)applicationDidFinishLaunching:(NSNotification *)notification {
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
    ".incbin \"Platform/MacOS/app_icon.png\"\n"
    "__app_icon_end:\n");

int main(int argc, const char * argv[]) {
    if(dndc_init_python() != 0)
        return 1;
    NSApplication* app = [NSApplication sharedApplication];
    DndAppDelegate* appDelegate = [DndAppDelegate new];
    app.delegate = appDelegate;
    auto icon_size = _app_icon_end - _app_icon;
    NSData* imagedata = [NSData dataWithBytesNoCopy:(void*)_app_icon length:icon_size freeWhenDone:NO];
    appimage = [[NSImage alloc] initWithData:imagedata];
    app.applicationIconImage = appimage;
    return NSApplicationMain(argc, argv);
}

static void do_menus(void){

    if (NSApp == nil) {
        return;
    }

    NSMenu *mainMenu = [[NSMenu alloc] init];
    /* Create the main menu bar */
    [NSApp setMainMenu:mainMenu];

    {
        NSString *appName = @"Gdndc";
        /* Create the application menu */
        NSMenu *appleMenu = [[NSMenu alloc] initWithTitle:@""];

        /* Add menu items */
        NSString *title = [@"About " stringByAppendingString:appName];
        [appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

        [appleMenu addItem:[NSMenuItem separatorItem]];

        [appleMenu addItemWithTitle:@"Preferences…" action:nil keyEquivalent:@","];

        [appleMenu addItem:[NSMenuItem separatorItem]];

        NSMenu* serviceMenu = [[NSMenu alloc] initWithTitle:@""];
        NSMenuItem *menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:serviceMenu];

        [NSApp setServicesMenu:serviceMenu];

        [appleMenu addItem:[NSMenuItem separatorItem]];

        title = [@"Hide " stringByAppendingString:appName];
        [appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

        menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [menuItem setKeyEquivalentModifierMask:(NSEventModifierFlagOption|NSEventModifierFlagCommand)];

        [appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

        [appleMenu addItem:[NSMenuItem separatorItem]];

        title = [@"Quit " stringByAppendingString:appName];
        [appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

        /* Put menu into the menubar */
        menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:appleMenu];
        [[NSApp mainMenu] addItem:menuItem];
    }

    /* Create the File menu */
    {
        NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
        [fileMenu addItemWithTitle:@"New Tab" action:@selector(newWindowForTab:) keyEquivalent:@"n"];
        [fileMenu addItemWithTitle:@"New Tab" action:@selector(newWindowForTab:) keyEquivalent:@"t"];
        [fileMenu addItemWithTitle:@"Open" action:@selector(openDocument:) keyEquivalent:@"o"];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        [fileMenu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
        [fileMenu addItemWithTitle:@"Save" action:@selector(saveDocument:) keyEquivalent:@"s"];
        [fileMenu addItemWithTitle:@"Revert to Saved" action:@selector(revertDocumentToSaved:) keyEquivalent:@"r"];
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:fileMenu];
        [[NSApp mainMenu] addItem:menuItem];
    }
    /* Create the edit menu */
    {
        NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
        [editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
        [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
        [editMenu addItem:[NSMenuItem separatorItem]];
        [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
        [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
        [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
        [editMenu addItemWithTitle:@"Indent" action:@selector(indent:) keyEquivalent:@">"];
        [editMenu addItemWithTitle:@"Dedent" action:@selector(dedent:) keyEquivalent:@"<"];
        [editMenu addItem:[NSMenuItem separatorItem]];
        [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
        [editMenu addItemWithTitle:@"Format" action:@selector(format_dnd:) keyEquivalent:@"J"];
        // Idk how to do this.
        NSMenuItem* mi = [[NSMenuItem alloc] initWithTitle:@"Find..." action:@selector(performTextFinderAction:) keyEquivalent:@"f"];
        mi.tag = NSTextFinderActionShowFindInterface;
        [editMenu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Find and Replace..." action:@selector(performTextFinderAction:) keyEquivalent:@"F"];
        mi.tag = NSTextFinderActionShowReplaceInterface;
        [editMenu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Find Next" action:@selector(performTextFinderAction:) keyEquivalent:@"g"];
        mi.tag = NSTextFinderActionNextMatch;
        [editMenu addItem:mi];

        mi = [[NSMenuItem alloc] initWithTitle:@"Find Previous" action:@selector(performTextFinderAction:) keyEquivalent:@"G"];
        mi.tag = NSTextFinderActionPreviousMatch;
        [editMenu addItem:mi];

        // [editMenu addItemWithTitle:@"Find..." action:@selector(performTextFinderAction:) keyEquivalent:@"f"];
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:editMenu];
        [[NSApp mainMenu] addItem:menuItem];
    }
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Insert"];
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];

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

        [menuItem setSubmenu:menu];
        [[NSApp mainMenu] addItem:menuItem];
    }
    /* Create the view menu */
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"View"];
        [menu addItemWithTitle:@"Toggle Editor" action:@selector(toggle_editor:) keyEquivalent:@"j"];
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Zoom Out" action:@selector(zoom_out:) keyEquivalent:@"-"];
        [menu addItemWithTitle:@"Zoom In" action:@selector(zoom_in:) keyEquivalent:@"+"];
        [menu addItemWithTitle:@"Actual Size" action:@selector(zoom_normal:) keyEquivalent:@"0"];

        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:menu];
        [[NSApp mainMenu] addItem:menuItem];
    }


    /* Create the window menu */
    {
        NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

        /* Add menu items */

        [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];

        [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

        /* Put menu into the menubar */
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:windowMenu];
        [[NSApp mainMenu] addItem:menuItem];

        /* Tell the application object that this is now the window menu */
        [NSApp setWindowsMenu:windowMenu];
    }
    /* Create the help menu */
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Help"];


        /* Put menu into the menubar */
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:menu];
        [[NSApp mainMenu] addItem:menuItem];
        /* Tell the application object that this is now the help menu */
        [NSApp setHelpMenu:menu];
    }
}

#include "allocator.c"
