//
// Copyright © 2021-2022, David Priver <david@davidpriver.com>
//
#import <Cocoa/Cocoa.h>
#import <Webkit/WebKit.h>
#ifndef DNDC_API
#define DNDC_API static inline
#endif
#import "Dndc/dndc_long_string.h"
#import "Dndc/common_macros.h"
#import "Utils/measure_time.h"
#import "Dndc/dndc.h"
#import "Dndc/dndc_ast.h"
#import "Utils/MStringBuilder.h"
#import "Allocators/mallocator.h"
#import "Utils/msb_format.h"
#import "Dndc/dndc_funcs.h"
#import "Dndc/dndc_credits.h"
#import "Utils/hash_func.h"
#import "filetree.h"
#import "filewatchcache.h"
#import "nsstr_util.h"
#define LOGIT(...) NSLog(@ "%d: " #__VA_ARGS__ "= %@", __LINE__, __VA_ARGS__)
// Convenience macro for writing inline javascript without a million quotes.
// Note that you need to semi-colon terminate all of your lines.
#define JSRAW(...) #__VA_ARGS__

#ifndef DNDC_DEVELOPER
#if 0
#define DNDC_DEVELOPER
#endif
#endif


#if !__has_feature(objc_arc)
#error "ARC is off"
#endif

#pragma clang assume_nonnull begin
static DndcWorkerThread*_Nonnull B64WORKER = nil;
static NSFont* EDITOR_FONT = nil;

//
// Setup menus without needing a nib (xib? whatever).
//
static void do_menus(void);

//
// Set up the appropriate syntax coloring.
//
static void do_syntax_colors(void);
//
// This is for detecting indent for smart indent
static NSString * const kIndentPatternString = @"^(\\t|\\s)+";
// ditto
static NSRegularExpression* indent_pattern;
//
// The app's image. We embed the png into the binary and decode it at startup.
static NSImage* appimage;
//
// Name of the app
NSString* APPNAME = @"DndEdit";

static
NSWindow* _Nullable
get_main_window(void){
    if(!NSApp) return nil;
    NSWindow* w = NSApp.mainWindow;
    if(!w) return nil;
    return w.parentWindow?:w;
}

//
// So, each document has N window controllers (I guess 1 for me).
// Each window controller has a window.
// Each window has a viewcontroller.
// Each viewcontroller will have a webview and a textview.
// The textview has a storage which has a highlighter.
// The webview has a webnavdel.
//
// The doccontroller controls the docs.
//

//
// Tags for inserting img blocks
typedef enum GdndInsertTag {
    GDND_INSERT_IMG,
    GDND_INSERT_IMGLINKS,
    GDND_INSERT_SCRIPT,
    GDND_INSERT_CSS,
    GDND_INSERT_DND,
} GdndInsertTag;

@interface DndWindowController: NSWindowController
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
-(instancetype)initWithFrame:(NSRect)textrect font:(NSFont*)font;
@end

@class DndDocument;
@class DndWebViewController;
//
// Delegate for the above textview that provides syntax highlighting
// for the .dnd file
//
@interface DndHighlighter: NSObject<NSTextStorageDelegate>
@property(weak, nonatomic) DndDocument* doc;
-(void)do_highlight:(NSTextStorage*)textStorage range:(NSRange)editedRange;
@end

//
// Controls what urls to allow (basically makes it so links will open a new
// .dnd document)
//
@interface WebNavDel : NSObject <WKNavigationDelegate>
@property(weak, nonatomic) DndWebViewController* controller;
@end

@interface DndUrlHandler: NSObject <WKURLSchemeHandler>
@property(weak, nonatomic) DndWebViewController* controller;
@end

//
// ViewController for the windows of the app
//
@interface DndEditViewController: NSViewController <NSToolbarDelegate, NSTextViewDelegate> {
@public NSScrollView* scrollview; // contains the text
@public NSSplitView* editor_container;
@public NSTextView* error_text;
@public DndHighlighter* highlighter; // for the text
@public NSToolbar* toolbar;
}
@property(weak, nonatomic) DndTextView* text;
@property(weak, nonatomic) DndDocument* doc;
-(instancetype)initWithDoc:(DndDocument*)doc withRect:(NSRect)textrect;
-(void)scroll_to_line:(int)line column:(int)column;
@end

@interface DndWebViewController: NSViewController <WKUIDelegate, NSToolbarDelegate>{
@public WKWebView* webview;
@public WebNavDel* webnavdel; // for the webview
@public DndUrlHandler* handler;
@public NSString* scroll_resto_string;
@public BOOL dont_update;
@public NSToolbar* toolbar;
}
@property(weak, nonatomic) DndDocument* doc;
-(NSURL*) this_dnd_url;
-(void)recalc_html:(NSString*)text;
-(void)save_scroll_position;
-(instancetype)initWithDoc:(DndDocument*)doc;
@end

//
// The Dnd document
//
@interface DndDocument: NSDocument{
// this is kind of janky, but whatever
@public DndTextView* text;
@public NSString* doc_title;
@public BOOL auto_recalc;
@public BOOL show_errors;
@public BOOL show_stats;
@public BOOL read_only;
@public BOOL coord_helper;
DndWebViewController* web_controller;
DndEditViewController* editor_controller;
NSWindow* editor_window;
NSWindow* web_window;
}
-(NSString*)get_text;
-(void)recalc_html;
-(void)change_font:(NSFont*)font;
-(void)update_coord:(NSString*)text;
-(void)scroll_to_id:(NSString*)nodeid;
@end

// The App delegate!
@interface DndAppDelegate : NSObject<NSApplicationDelegate>
-(void)openFolder:(nullable id) sender;
@end

@implementation DndDocument
+(BOOL)autosavesInPlace {
    return YES;
}
-(instancetype)init {
    self = [super init];
    self->coord_helper = NO;
    self->auto_recalc = YES;
    self->show_errors = YES;
    self->show_stats = NO;
    self->web_controller = [[DndWebViewController alloc] initWithDoc:self];
    NSScreen* screen = NSScreen.mainScreen;
    NSRect screenrect;
    if(screen)
        screenrect = screen.visibleFrame;
    else
        screenrect = NSMakeRect(0, 0, 1400, 800);
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:(id)EDITOR_FONT, NSFontAttributeName, nil];
    CGFloat Msize = [[NSAttributedString alloc] initWithString:@"M" attributes:attributes].size.width;
    CGFloat textwidth  = fmax(84*Msize, 800);
    NSRect textrect = {.origin={0, 0}, .size={textwidth,screenrect.size.height}};
    self->text = [[DndTextView alloc] initWithFrame:textrect font:EDITOR_FONT];
    return self;
}
-(nullable instancetype)initForURL:(NSURL *_Nullable)urlOrNil
    withContentsOfURL:(NSURL *)contentsURL
    ofType:(NSString *)typeName
    error:(NSError * _Nullable *)outError{
    self = [super initForURL:urlOrNil withContentsOfURL:contentsURL ofType:typeName error:outError];
    if(!self) return nil;
    [self recalc_html];
    return self;
}
-(nullable instancetype)initWithContentsOfURL:(NSURL *)url ofType:(NSString *)typeName error:(NSError **)outError{
    self = [super initWithContentsOfURL:url ofType:typeName error:outError];
    [self recalc_html];
    return self;
}

-(NSWindow*)make_window{
    NSScreen* screen = NSScreen.mainScreen;
    NSRect rect = screen? screen.visibleFrame : NSMakeRect(0, 0, 1400, 800);

    NSWindow* window = [[NSWindow alloc]
        initWithContentRect: rect
        styleMask: NSWindowStyleMaskTitled
                 | NSWindowStyleMaskClosable
                 | NSWindowStyleMaskResizable
        backing: NSBackingStoreBuffered
        defer: NO];
    window.title = @"Dndc";
    window.titlebarAppearsTransparent = YES;
    window.tabbingIdentifier = @"Dnd Window";
    window.contentViewController = self->web_controller;
    window.toolbar = self->web_controller->toolbar;

    self->web_window = window;
    return window;
}
-(void)makeWindowControllers{
    NSWindow* docwindow = [self make_window];
    DndWindowController* winc = [[DndWindowController alloc] initWithWindow:docwindow];
    [self addWindowController:winc];
    NSWindow* mainwindow = get_main_window();
    // [self recalc_html];
    if(mainwindow){
        [mainwindow addTabbedWindow:docwindow ordered:NSWindowAbove];
    }
    // [docwindow makeKeyAndOrderFront: nil];
}
-(void)showWindows{
    [super showWindows];
}

- (nullable NSData*)dataOfType:(NSString *)typeName error:(NSError **)outError {
    return [[text string] dataUsingEncoding:NSUTF8StringEncoding];
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError {
    NSString* str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if(str){
        text.string = str;
    }
    return YES;
}
-(void) pop_out_editor:(nullable id) sender{
    (void)sender;
    if(self->editor_window && self->editor_window.isVisible){
        [editor_window close];
        return;
    }
    if(!self->editor_window){
        self->editor_window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, 800, 1000)
            styleMask: NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskTitled
            backing: NSBackingStoreBuffered
            defer: YES];
        self->editor_window.releasedWhenClosed = NO;
        [self->editor_window standardWindowButton:NSWindowZoomButton].enabled = NO;
        assert(!self->editor_controller);
        self->editor_controller = [[DndEditViewController alloc] initWithDoc:self withRect:NSMakeRect(0, 0, 800, 1000)];
        self->editor_window.contentView = self->editor_controller->editor_container;
        [self->editor_controller->highlighter do_highlight:self->text.textStorage range:NSMakeRange(0, self->text.textStorage.length)];
        [self->editor_controller->editor_container adjustSubviews];
        [self->editor_window center];
        self->editor_window.toolbar = self->editor_controller->toolbar;
    }
    [self->web_window addChildWindow:self->editor_window ordered:NSWindowAbove];
    [self->editor_window makeKeyAndOrderFront:nil];
    DndWindowController* wc = [[DndWindowController alloc] initWithWindow:self->editor_window];
    [self addWindowController:wc];
}
-(void)scroll_to_line:(int)line column:(int)column{
    if(!self->editor_window || !self->editor_window.isVisible)
        [self pop_out_editor:nil];
    [self->editor_controller scroll_to_line:line column:column];
}
-(void)change_font:(NSFont*)font{
    self->text.font = font;
}

-(void)recalc_html{
    // NSLog(@"%@", NSThread.callStackSymbols);
    // LOGIT(@"recalcing html");
    [web_controller recalc_html:[self get_text]];
}
-(NSString*)get_text{
    [self->web_controller save_scroll_position];
    NSString *string = self->text.string;
    // Inject javascript that will restore the scroll position in the window.
    string = [string stringByAppendingString:
        @"\n"
        "::script\n"];
    if(!self->coord_helper)
        string = [string stringByAppendingString:
            @"\n"
            "::script\n  "
            // Internal anchors are broken in some versions of webkit. Inject
            // this function to recreate the wanted behavior.
            JSRAW(document.addEventListener("DOMContentLoaded", function(){
                const anchors = document.getElementsByTagName('a');
                function add_interceptor(a){
                    a.onclick = function(e){
                        let href = a.href;
                        if(href.baseVal) href = href.baseVal;
                        let split = href.split('#');
                        if(split.length > 1){
                            let target = split[1];
                            let t = document.getElementById(target);
                            if(t){
                                t.scrollIntoView();
                                e.preventDefault();
                                e.stopPropagation();
                                fetch("dnd:///scrolltoid", {method:"POST", body:target});
                                return false;
                            }
                        }
                        a.setAttribute('target', '_blank');
                    };
                }
                for(let a of anchors){
                    add_interceptor(a);
                }
                function add_scroller(h){
                    if(!h.id) return;
                    h.style.cursor = "pointer";
                    h.onclick = function(e){
                        e.preventDefault();
                        e.stopPropagation();
                        fetch("dnd:///scrolltoid", {method:"POST", body:h.id});
                        return false;
                    };
                }
                for(let h of document.getElementsByTagName("h2"))
                    add_scroller(h);
                for(let h of document.getElementsByTagName("h3"))
                    add_scroller(h);
            });)
            "\n"
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
    if(self->coord_helper){
        string = [string stringByAppendingString:
            @"\n"
            "::script\n  "
            JSRAW(
            document.addEventListener("DOMContentLoaded", function(){
              const svgs = document.getElementsByTagName("svg");
              let moving = false;
              for(let i = 0; i < svgs.length; i++){
                const svg = svgs[i];
                const texts = svg.getElementsByTagName("text");
                const aa = document.querySelectorAll("svg a");
                for(let text of texts){
                    text.parentNode.addEventListener("click", function(e){
                        e.preventDefault();
                        e.stopPropagation();
                    });
                }
                for(let i = 0; i < texts.length; i++){
                    let anchor = texts[i];
                    anchor.addEventListener("pointerdown", function(e){
                        e.stopPropagation();
                        e.preventDefault();
                        if(moving) return;
                        moving = true;
                        let svg = anchor.parentElement.parentElement;
                        let sx = svg.width.baseVal.value / svg.viewBox.baseVal.width;
                        let sy = svg.height.baseVal.value / svg.viewBox.baseVal.height;
                        let org_x = anchor.transform.baseVal[0].matrix.e | 0;
                        let org_y = anchor.transform.baseVal[0].matrix.f | 0;
                        let start_x = e.screenX;
                        let start_y = e.screenY;
                        function move(e){
                            let diffx = 1/sx*(e.screenX - start_x);
                            let diffy = 1/sy*(e.screenY - start_y);
                            start_x = e.screenX;
                            start_y = e.screenY;
                            anchor.transform.baseVal[0].matrix.e += diffx;
                            anchor.transform.baseVal[0].matrix.f += diffy;
                        }
                        svg.addEventListener("pointermove", move);
                        function remove(e){
                            moving = false;
                            e.stopPropagation();
                            e.preventDefault();
                            svg.removeEventListener('pointermove', move);
                            let a = anchor.parentElement;
                            let href = a.href.baseVal;
                            let internal_id = 0;
                            let sp = href.split('#');
                            if(sp.length > 1)
                                internal_id = _coords[sp[1]];
                            if(!internal_id)
                                internal_id = _coords2[href];
                            if(!internal_id){
                                let t = anchor.childNodes[0].textContent.trim();
                                console.log('t', t);
                                internal_id = _coords2[t];
                            }
                            if(!internal_id) return;
                            let request = new XMLHttpRequest();
                            let new_x = anchor.transform.baseVal[0].matrix.e | 0;
                            let new_y = anchor.transform.baseVal[0].matrix.f | 0;
                            const combo = `${internal_id}:${new_x},${new_y}`;
                            fetch("dnd:///roommove", {method:"POST", body:combo});
                        }
                        window.addEventListener("pointerup", remove, {once:true});
                    });
                }
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
                    if(!name.includes(".")){
                      name += ".";
                    }
                    const combined = "\n"+name+":"+":md .room @coord("+true_x+","+true_y+")\n";
                    request.open("POST", "dnd:///roomclick", true);
                    request.send(combined);
                  }
                });
              }
            });
            )
            "\n"
            "::js\n  "
            JSRAW(
                let coords = ctx.select_nodes({attributes:["coord"]});
                let s = ctx.root.make_child(NodeType.SCRIPTS);
                let o = {};
                for(let co of coords){
                    o[co.id] = co.internal_id;
                }
                s.make_child(NodeType.STRING, {header:`let _coords = ${JSON.stringify(o)};`});
                let imglinks = ctx.select_nodes({type:NodeType.IMGLINKS});
                let o2 = {};
                for(let il of imglinks){
                    for(let ch of il.children){
                        if(ch.type != NodeType.STRING) continue;
                        let lead = ch.header.split('=')[0].trim();
                        o2[lead] = ch.internal_id;
                    }
                }
                s.make_child(NodeType.STRING, {header:`let _coords2 = ${JSON.stringify(o2)};`});
            )
        ];
    }
    return string;
}

-(void)update_coord:(NSString*)coord_text{
    // LOGIT(coord_text);
    // coord_text is of the format {internal_id}:{new x},{new y}
    NSArray<NSString*>* parts = [coord_text componentsSeparatedByString:@":"];
    if(parts.count != 2) return;
    int internal_id = parts[0].intValue;
    NSString* doc_string = self->text.string;
    DndcContext* ctx = dndc_create_ctx(0, NULL, NULL);
    dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, SV(""));
    int err;
    err = dndc_ctx_parse_string(ctx, root, SV(""), ns_borrow_sv(doc_string));
    if(err) goto fail;
    StringView c = ns_borrow_sv(parts[1]);
    err = dndc_node_set_attribute(ctx, internal_id, SV("coord"), c);
    if(err) goto fail;
    {
        StringView header;
        err = dndc_node_get_header(ctx, internal_id, &header);
        if(err) goto fail;
        {
            const char* at;
            if(header.length < 256 && (at = memchr(header.text, '@', header.length))){
                char buffer[512];
                int length = snprintf(buffer, sizeof buffer, "%.*s@ %.*s", (int)(at - header.text), header.text, (int)c.length, c.text);
                StringView h = {.text=buffer, .length=length};
                err = dndc_ctx_dup_sv(ctx, h, &h);
                if(err) goto fail;
                dndc_node_set_header(ctx, internal_id, h);
            }
        }
    }
    {
        LongString expanded;
        err = dndc_ctx_format_tree(ctx, &expanded);
        if(err) goto fail;
        self->text.string = ns_consume_ls(expanded);
    }
    [self->text scrollToBeginningOfDocument:self];
    dndc_ctx_destroy(ctx);
    return;

    fail:
    // LOGIT(@"Failed");
    dndc_ctx_destroy(ctx);
    return;
}

-(void)scroll_to_id:(NSString*)nodeid{
    NSString* doc_string = self->text.string;
    DndcContext* ctx = dndc_create_ctx(0, NULL, NULL);
    dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, SV(""));
    int err;
    err = dndc_ctx_parse_string(ctx, root, SV(""), ns_borrow_sv(doc_string));
    if(err) goto fail;
    StringView nid = ns_borrow_sv(nodeid);
    DndcNodeHandle target = dndc_ctx_node_by_id(ctx, nid);
    if(target == DNDC_NODE_HANDLE_INVALID) goto fail;
    DndcNodeLocation loc; err = dndc_node_location(ctx, target, &loc);
    if(err) goto fail;
    size_t length = doc_string.length;
    size_t lineno = 0;
    size_t i;
    for(i = 0; i < length; i++){
        if([doc_string characterAtIndex:i] == u'\n'){
            lineno++;
            if(lineno == loc.row) break;
        }
    }
    if(lineno != loc.row) goto fail;
    {
    NSRange range = NSMakeRange(i, 0);
    // [self->text scrollRangeToVisible:range];
    NSLayoutManager *l = self->text.layoutManager;
    NSRect rect = [l boundingRectForGlyphRange:range inTextContainer:self->text.textContainer];
    rect = NSOffsetRect(rect, self->text.textContainerOrigin.x, self->text.textContainerOrigin.y);
    CGPoint point = rect.origin;
    [self->text scrollPoint:point];
    dndc_ctx_destroy(ctx);
    return;
    }
fail:
    dndc_ctx_destroy(ctx);
}
-(void)refresh{
    [self recalc_html];
}
-(void)scroll_selection_into_view:(id _Nullable) sender{
    NSRange r = self->text.selectedRange;
    // this is ridiculous
    int lineno = 1;
    {
        NSString* s = self->text.string;
        for(NSUInteger i = 0; i < r.location; i++){
            unichar c = [s characterAtIndex:i];
            if(c == u'\n') lineno++;
        }
    }
    DndcContext* ctx = dndc_create_ctx(0, NULL, NULL);
    dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, SV(""));
    int err;
    NSString* doc_string = self->text.string;
    err = dndc_ctx_parse_string(ctx, root, SV("a"), ns_borrow_sv(doc_string));
    if(err) goto cleanup;
    DndcNodeHandle nh = dndc_ctx_node_by_approximate_location(ctx, SV("a"), lineno, 0);
    // fprintf(stderr, "%d nh: %zu, root: %zu\n", __LINE__, (size_t)nh, (size_t)root);
    while(nh != DNDC_NODE_HANDLE_INVALID && nh != root && !dndc_node_has_id(ctx, nh)){
        nh = dndc_node_get_parent(ctx, nh);
    }
    // fprintf(stderr, "%d nh: %zu, root: %zu\n", __LINE__, (size_t)nh, (size_t)root);
    if(nh == DNDC_NODE_HANDLE_INVALID) goto cleanup;
    if(nh == root) goto cleanup;

    DndcStringView idstr;
    err = dndc_node_get_id(ctx, nh, &idstr);
    if(err) goto cleanup;
    // fprintf(stderr, "idstr: '%.*s'\n", (int)idstr.length, idstr.text);
    if(idstr.length > 509) goto cleanup;
    char buff[512];
    size_t buffused = 0;
    err = dndc_kebab(idstr, buff, sizeof buff, &buffused);
    if(err) goto cleanup;
    if(!buffused) goto cleanup;
    buff[buffused] = 0;
    // fprintf(stderr, "buff: '%.*s'\n", (int)buffused, buff);
    if(0){
        cleanup:
        // fprintf(stderr, "lineno: %d\n", lineno);
        dndc_ctx_destroy(ctx);
        return;
    }
    dndc_ctx_destroy(ctx);
    NSString* script = [NSString stringWithFormat:@""
        "(function(){\n"
        "  let node = document.getElementById('%.*s');\n"
        "  if(!node) return;\n"
        "  node.scrollIntoView(true);\n"
        "})();\n"
        , (int)buffused, buff];
    [self->web_controller->webview evaluateJavaScript:script completionHandler:^(id object, NSError* error){
        if(error) LOGIT(error);
        (void)object;
    }];
}
@end


@interface DndLink: NSObject {
    @public NSString* filename;
    @public int line;
    @public int col;
}
@end
@implementation DndLink
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
dndc_syntax_func(void* _Nullable data, int type, int line, int col, const uint16_t*begin, size_t length){
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
        if(node->header.length){
            MStringBuilder sb = {.allocator = MALLOCATOR};
            msb_write_str(&sb, node->header.text, node->header.length);
            NSString* str = msb_detach_as_ns_string(&sb);
            DndDocument* vc = (__bridge DndDocument*)data;
            vc->doc_title = str;
        }
    }
    {
        BOOL has_darkmode_css = 0;
        // XXX: should we add fast paths for nodes by type in public API?
        MARRAY_FOR_EACH(NodeHandle, h, ctx->stylesheets_nodes){
            Node* node = get_node(ctx, *h);
            NODE_CHILDREN_FOR_EACH(c, node){
                Node* ch = get_node(ctx, *c);
                if(memmem(ch->header.text, ch->header.length, "prefers-color-scheme", sizeof("prefers-color-scheme")-1)){
                    has_darkmode_css = 1;
                    goto after;
                }
            }
        }
        after:;
        if(!has_darkmode_css){
            DndcNodeHandle h = dndc_ctx_make_node(ctx, NODE_STYLESHEETS, SV(""), ctx->root_handle._value);
            if(h != DNDC_NODE_HANDLE_INVALID){
                dndc_node_append_string(ctx, h, SV(JSRAW(
                    @media (prefers-color-scheme: dark) {
                        html {
                            color-scheme: dark;
                            color: #aaa;
                            background-color: #222;
                        }
                        svg {
                            background-color: white;
                        }
                        tr:nth-child(even){
                          background: #333;
                        }
                        h3, h4, h5 {
                            color: rgb(161, 26, 2)
                        }
                        a {
                            color: white;
                        }
                    }
                )));
            }
        }
    }
    return 0;
}
static
void
gdndc_error_func(void* _Nullable data, int type, const char*_Nonnull filename, int filename_len, int line, int col, const char*_Nonnull message, int message_len){
    if(!data)
        return;
    NSTextView* tv = (__bridge NSTextView*)data;
    MStringBuilder builder = {.allocator=MALLOCATOR};
    StringView fn = {
        .text = filename,
        .length = filename_len,
    };
    StringView mess = {
        .text = message,
        .length = message_len,
    };
    switch((enum DndcLogMessageType)type){
        case DNDC_ERROR_MESSAGE:
        case DNDC_WARNING_MESSAGE:
            if(SV_equals(fn, SV("(string input)")))
                MSB_FORMAT(&builder, line, ":", col, ": ", mess, "\n");
            else
                MSB_FORMAT(&builder, fn, ":", line, ":", col, ": ", mess, "\n");
            break;
        case DNDC_NODELESS_MESSAGE:
        case DNDC_STATISTIC_MESSAGE:
        case DNDC_DEBUG_MESSAGE:
            MSB_FORMAT(&builder, mess, "\n");
            break;
    }
    NSString* s = msb_detach_as_ns_string(&builder);
    NSMutableAttributedString* as = [[NSMutableAttributedString alloc] initWithString:s];
    if(fn.length && !SV_equals(fn, SV("(string input)"))){
        NSRange range = NSMakeRange(0, fn.length); // XXX this is in utf8, should be utf16 length
        DndLink* link = [[DndLink alloc] init];
        link->filename = [NSString stringWithFormat:@"/%.*s", (int)fn.length, fn.text];
        link->line = line;
        link->col = col;
        [as addAttribute:NSLinkAttributeName value:link range:range];
    }
    [tv.textStorage appendAttributedString:as];
}

@implementation DndHighlighter
- (void)textStorage:(NSTextStorage *)textStorage
  didProcessEditing:(NSTextStorageEditActions)editedMask
              range:(NSRange)editedRange
     changeInLength:(NSInteger)delta{
    if(!(editedMask & NSTextStorageEditedCharacters)) return;
    if(self.doc)
        [self.doc recalc_html];
    [self do_highlight:textStorage range:editedRange];
}
-(void)do_highlight:(NSTextStorage*)textStorage range:(NSRange)editedRange{
    NSString *string = textStorage.string;
    LongString text;
    text.text = string.UTF8String;
    text.length = strlen(text.text);
    // NSRange currentLineRange = NSMakeRange(0, [string length]);
    NSRange currentLineRange = [string lineRangeForRange:editedRange];
    // uint64_t before= get_t();
    [textStorage removeAttribute:NSForegroundColorAttributeName range:currentLineRange];
    [textStorage removeAttribute:NSBackgroundColorAttributeName range:currentLineRange];
    size_t len = [string length];
    // XXX: gross! not mt-safe
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
    dndc_analyze_syntax_utf16(text16, dndc_syntax_func, &sd);
}
@end

@implementation DndTextView
-(void)keyDown:(NSEvent*) event{
    if(event.keyCode == 53){
        [self.window close];
    }
    if(event.modifierFlags & NSEventModifierFlagCommand){
        NSInteger num = event.characters.integerValue;
        if(num){
            num -= 1;
            NSArray<NSWindow*>* tabs = get_main_window().tabbedWindows;
            if(tabs.count > num){
                NSWindow* tab = tabs[num];
                [tab makeKeyAndOrderFront:nil];
                if(tab.childWindows && tab.childWindows.count)
                    [tab.childWindows[0] makeKeyAndOrderFront:nil];
                return;
           }
        }
    }
    [super keyDown:event];
}

-(void)indent:(id _Nullable)sender{
    NSRange r = self.selectedRange;
    NSRange currentLineRange = [self.string lineRangeForRange:r];
    int adjustment = 0;
    [self insertText:@"  " replacementRange:NSMakeRange(currentLineRange.location, 0)];
    adjustment += 2;
    //               -1 to not do final newline
    for(int i = 0; i < currentLineRange.length-1; i++){
        unichar c = [self.string characterAtIndex:i+currentLineRange.location+adjustment];
        if(c == '\n'){
            [self insertText:@"  " replacementRange:NSMakeRange(currentLineRange.location + i + 1 + adjustment, 0)];
            adjustment += 2;
        }
    }
    NSRange adjustedrange = NSMakeRange(currentLineRange.location, currentLineRange.length+adjustment);
    if(r.length > 0)
        [self setSelectedRange:adjustedrange];
}
-(void)dedent:(id _Nullable)sender{
    NSRange r = self.selectedRange;
    NSRange currentLineRange = [self.string lineRangeForRange:r];
    int adjustment = 0;
    int n_leading_space = 0;  // negative means no longer counting leading spaces.
    for(int i = 0; i < currentLineRange.length-1; i++){
        unichar c = [self.string characterAtIndex:i+currentLineRange.location+adjustment];
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
                NSRange range = NSMakeRange(currentLineRange.location+adjustment+i-1, 2);
                [self insertText:@"" replacementRange:range];
                n_leading_space = -1;
                adjustment -= 2;
            }
            continue;
        }
        n_leading_space = -1;
    }
    NSRange adjustedrange = NSMakeRange(currentLineRange.location, currentLineRange.length+adjustment);
    if(r.length > 0)
        self.selectedRange = adjustedrange;
}
-(void)ensure_pattern{
    if(!indent_pattern)
        PushDiagnostic();
        #pragma clang diagnostic ignored "-Wnullable-to-nonnull-conversion"
        indent_pattern = [[NSRegularExpression alloc] initWithPattern:kIndentPatternString options:0 error:nil];
        PopDiagnostic();
}
-(void)insert_file_block:(NSString*)path tag:(GdndInsertTag)tag size:(NSSize)size{
    NSRange r = self.selectedRange;
    NSRange currentLineRange = [self.string lineRangeForRange:r];
    NSUInteger rel = r.location - currentLineRange.location;
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
    MStringBuilder sb = {.allocator=MALLOCATOR};
    int err = msb_ensure_additional(&sb, 256);
    unhandled_error_condition(err);
    msb_write_str(&sb, blockname.text, blockname.length);
    msb_write_nchar(&sb, ' ', indent_amount+2);
    const char* cpath = path.UTF8String;
    msb_write_str(&sb, cpath, strlen(cpath));
    msb_write_char(&sb, '\n');
    NSString* to_insert = msb_detach_as_ns_string(&sb);
    [self insertText:to_insert replacementRange:r];
}
-(void)insert_imglinks_block:(NSString*)path at:(NSRange)r indent_amount:(NSInteger)indent_amount size:(NSSize)size{
    MStringBuilder sb = {.allocator = MALLOCATOR};
    int err = msb_ensure_additional(&sb, 256);
    unhandled_error_condition(err);
    msb_write_literal(&sb, "::imglinks\n");
#define INDENT() msb_write_nchar(&sb, ' ', indent_amount+2)
    const char* imgpath = [path UTF8String];
    INDENT(); msb_write_str(&sb, imgpath, strlen(imgpath));
    msb_write_char(&sb, '\n');
    double scale = size.width > size.height? 800.0/size.width : 800.0/size.height;
    INDENT(); MSB_FORMAT(&sb, "width = ", (int)(size.width*scale), "\n");
    INDENT(); MSB_FORMAT(&sb, "height = ", (int)(size.height*scale), "\n");
    INDENT(); MSB_FORMAT(&sb, "viewBox = 0 0 ", (int)size.width, " ", (int)size.height, "\n");
    StringView script[] = {
        SV("::js\n"),
        SV("  // this is an example of how to script the imglinks\n"),
        SV("  let imglinks = node.parent;\n"),
        SV("  let coord_nodes = ctx.select_nodes({attributes:['coord']});\n"),
        SV("  for(let c of coord_nodes){\n"),
        SV("    let lead = c.header;\n"),
        SV("    let position = c.attributes.get('coord');\n"),
        SV("    imglinks.add_child(`${lead} = #${c.id} @${position}`);\n"),
        SV("  }\n"),
    };
    for(int i = 0; i < arrlen(script); i++){
        INDENT();
        msb_write_str(&sb, script[i].text, script[i].length);
    }
#undef INDENT
    NSString* to_insert = msb_detach_as_ns_string(&sb);
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

    [result addItem:NSMenuItem.separatorItem];

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

    item = [[NSMenuItem alloc] initWithTitle:@"Scroll Into View" action:@selector(scroll_selection_into_view:) keyEquivalent:@""];
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
-(instancetype)initWithFrame:(NSRect)textrect font:(NSFont*)font{
    self = [super initWithFrame:textrect];
    if(!self) return nil;
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
    self.minSize = textrect.size;
    self.maxSize = textrect.size;
    // self.minSize = NSMakeSize(0.0, textrect.size.height);
    self.maxSize = NSMakeSize(1e9, 1e9);
    self.verticallyResizable = YES;
    self.horizontallyResizable = NO;
    self.textContainer.containerSize = NSMakeSize(textrect.size.width, 1e9);
    self.textContainer.widthTracksTextView = YES;
    self.textContainerInset = NSMakeSize(4,4);
    self.usesFindBar = YES;
    self.incrementalSearchingEnabled = YES;
    return self;
}

-(void)deleteBackward:(id _Nullable)sender{
    NSRange r = self.selectedRange;
    if(r.length == 0){
        NSRange currentLineRange = [self.string lineRangeForRange:r];
        NSString* currentLine = [self.string substringWithRange:currentLineRange];
        [self ensure_pattern];
        NSTextCheckingResult* indent_matched = [indent_pattern firstMatchInString:currentLine options:0 range:NSMakeRange(0, currentLine.length)];
        if(indent_matched){
            unichar this_char = r.location?[self.string characterAtIndex:r.location-1]:'\0';
            // how far into the line the current selection is
            NSUInteger rel = r.location - currentLineRange.location;
            if(rel && !(rel & 1) && this_char == ' ' && rel <= indent_matched.range.length){
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
    NSRange sel_range = self.selectedRange;
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
#define DND_HOST @"dndedit"
#define DND_SCHEME @"dnd"
#define DND_SCHEME_HOST @"dnd://dndedit"
@implementation WebNavDel: NSObject
-(void)webView:(WKWebView *)webView
    decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler{
        NSURL* url = navigationAction.request.URL;
        // LOGIT(url);
        if([url isEqual:self.controller.this_dnd_url]){
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }
        if([url.host isEqual:DND_HOST]){
            NSURL* real_url = [NSURL fileURLWithPath:[url.URLByDeletingPathExtension URLByAppendingPathExtension:@"dnd"].path];
            [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:real_url display:YES completionHandler:^(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error){
                (void)documentWasAlreadyOpen;
                (void)document;
                if(error && error.code == NSFileReadNoSuchFileError){
                    if([real_url.path isEqualToString:@"/nil.dnd"]){
                        decisionHandler(WKNavigationActionPolicyAllow);
                        return;
                    }
                    [NSTimer scheduledTimerWithTimeInterval:0.000 repeats:NO block:^(NSTimer* timer){
                        (void)timer;
                        NSAlert *alert = [[NSAlert alloc] init];
                        [alert setMessageText:@"File does not exist."];
                        [alert setInformativeText:[real_url.path stringByAppendingString:@" does not exist. Create it?"]];
                        [alert addButtonWithTitle:@"Ok"];
                        [alert addButtonWithTitle:@"Cancel"];
                        NSModalResponse response = [alert runModal];
                        if(response != NSAlertFirstButtonReturn)
                            return;
                        const char* cstring = real_url.path.UTF8String;
                        if(!cstring) return;
                        int fd = open(cstring, O_RDWR|O_CREAT|O_EXCL, 0644);
                        if(fd < 0) return;
                        if(close(fd) < 0){
                            perror("close");
                            return;
                        }
                        [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:real_url display:YES completionHandler:^(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error){
                            (void)documentWasAlreadyOpen;
                            (void)document;
                            if(error){
                                LOGIT(real_url);
                                LOGIT(error);
                            }
                        }];
                    }];
                    decisionHandler(WKNavigationActionPolicyAllow);
                    return;
                }
                else if(error){
                    LOGIT(real_url);
                    LOGIT(error);
                    decisionHandler(WKNavigationActionPolicyAllow);
                }
                else {
                    decisionHandler(WKNavigationActionPolicyCancel);
                }
            }];
            return;
        }
        // LOGIT(url);
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
}
@end

@implementation DndUrlHandler
-(void)webView:(WKWebView*)webView startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask{
    NSURLRequest* request = urlSchemeTask.request;
    NSURL* url = request.URL;
    NSString* method = request.HTTPMethod;
    // Handle the click helper for adding rooms by clicking
    // on map.
    if([method isEqualToString:@"POST"] && [url isEqual:[NSURL URLWithString:@"dnd:///roomclick"]]){
        NSURLResponse* response = [[NSURLResponse alloc]
            initWithURL:request.mainDocumentURL
            MIMEType:@"text/plain"
            expectedContentLength:0
            textEncodingName:nil];
        [urlSchemeTask didReceiveResponse:response];
        NSString* body = [[NSString alloc] initWithData:(NSData*)[request HTTPBody] encoding:NSUTF8StringEncoding];
        [urlSchemeTask didFinish];
        // append the body of the request to the document (click)
        [self.controller.doc->text insertText:body replacementRange:NSMakeRange(self.controller.doc->text.textStorage.length, 0)];
        return;
    }
    if([method isEqualToString:@"POST"] && [url isEqual:[NSURL URLWithString:@"dnd:///roommove"]]){
        // LOGIT(url);
        NSURLResponse* response = [[NSURLResponse alloc]
            initWithURL:request.mainDocumentURL
            MIMEType:@"text/plain"
            expectedContentLength:0
            textEncodingName:nil];
        [urlSchemeTask didReceiveResponse:response];
        NSString* body = [[NSString alloc] initWithData:(NSData*)[request HTTPBody] encoding:NSUTF8StringEncoding];
        // LOGIT(body);
        [urlSchemeTask didFinish];
        [self.controller.doc update_coord:body];
        return;
    }
    if([method isEqualToString:@"POST"] && [url isEqual:[NSURL URLWithString:@"dnd:///scrolltoid"]]){
        // LOGIT(url);
        NSURLResponse* response = [[NSURLResponse alloc]
            initWithURL:request.mainDocumentURL
            MIMEType:@"text/plain"
            expectedContentLength:0
            textEncodingName:nil];
        [urlSchemeTask didReceiveResponse:response];
        NSString* body = [[NSString alloc] initWithData:(NSData*)[request HTTPBody] encoding:NSUTF8StringEncoding];
        // LOGIT(body);
        [urlSchemeTask didFinish];
        [self.controller.doc scroll_to_id:body];
        return;
    }
    if([method isEqualToString:@"GET"]){
        if([url.scheme isEqualToString:DND_SCHEME] && [url.path isEqualToString:@"/scrollresto"]){
        NSURLResponse* response = [[NSHTTPURLResponse alloc]
            initWithURL:(NSURL*)[NSURL URLWithString:DND_SCHEME_HOST]
             statusCode:200
            HTTPVersion:@"HTTP/1.1"
           headerFields:@{
           @"Access-Control-Allow-Origin":DND_SCHEME_HOST,
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
    if([method isEqualToString:@"GET"] and [url.scheme isEqualToString:DND_SCHEME]){
        LOGIT([@"file://" stringByAppendingString:url.path]);
        NSString* urlstr = [url path];
        urlstr = [urlstr stringByAddingPercentEncodingWithAllowedCharacters: NSCharacterSet.URLPathAllowedCharacterSet];
        urlstr = [@"file://" stringByAppendingString:urlstr];
        url = [[NSURL alloc] initWithString: urlstr];
        // NSString* path = url.path;
        // LOGIT(url);
        NSData* data = [NSData dataWithContentsOfURL: url];
        if(!data){
            LOGIT(@"no data?");
            // TODO: better error.
            [urlSchemeTask didFailWithError:[NSError errorWithDomain:@"denied" code:1 userInfo:nil]];
        }
        NSURLReponse* response = [[NSURLResponse alloc]
            initWithURL: request.mainDocumentURL
            MIMEType:@"image/png" // This is wrong, but webkit doesn't mind.
            expectedContentLength: [data length]
            textEncodingName:nil];
        [urlSchemeTask didReceiveResponse:response];
        [urlSchemeTask didReceiveData:data];
        [urlSchemeTask didFinish];
        // int fd = open(path.UTF8String, O_EVTONLY);
        // if(fd > 0){
            // dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fd, DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_ATTRIB, dispatch_get_main_queue());
            // dispatch_source_set_event_handler(source, ^{
                    // TODO: figure out how to invalidate the webview cache.
                    // dispatch_cancel(source);
                // });
            // dispatch_source_set_cancel_handler(source, ^{
                    // close(fd);
                    // });
            // dispatch_resume(source);
        // }

    }
#endif
    [urlSchemeTask didFailWithError:[NSError errorWithDomain:@"denied" code:1 userInfo:nil]];
}
-(void)webView:(WKWebView*)webView stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask{
}
@end

@interface MyWebView: WKWebView
@end

@implementation MyWebView
-(void)keyDown:(NSEvent*) event{
    LOGIT(event);
    if(event.type == NSEventTypeKeyDown && event.keyCode == 31){
        if(filetree_window)
            [filetree_window makeKeyAndOrderFront:nil];
        else
            [(DndAppDelegate*)NSApp.delegate openFolder:nil];
        return;
    }
    [super keyDown:event];
}

@end


@implementation DndWebViewController
- (nullable NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    // LOGIT(itemIdentifier);
    NSToolbarItem* tbi = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier ];
    tbi.label = itemIdentifier;
    if([itemIdentifier isEqualToString:@"Edit"]){
        NSImage* img = [NSImage imageWithSystemSymbolName:@"square.and.pencil" accessibilityDescription:nil];
        tbi.target = self.doc;
        tbi.action = @selector(pop_out_editor:);
        tbi.image = img;
        tbi.navigational = YES;
        tbi.bordered = YES;
        tbi.label = @"Edit";
        tbi.toolTip = @"Edit This Document";
    }
    if([itemIdentifier isEqualToString:@"File Tree"]){
        NSImage* img = [NSImage imageWithSystemSymbolName:@"list.triangle" accessibilityDescription:nil];
        tbi.target = NSApp.delegate;
        tbi.action = @selector(show_file_tree:);
        tbi.image = img;
        tbi.navigational = YES;
        tbi.bordered = YES;
        tbi.label = @"File Tree";
        tbi.toolTip = @"Browse File Tree";
    }
    if([itemIdentifier isEqualToString:@"New Doc"]){
        NSImage* img = [NSImage imageWithSystemSymbolName:@"doc.badge.plus" accessibilityDescription:nil];
        // tbi.target = NSApp.delegate;
        tbi.action = @selector(newWindowForTab:);
        tbi.image = img;
        tbi.navigational = YES;
        tbi.bordered = YES;
        tbi.label = @"New Document";
        tbi.toolTip = @"New Document";
    }
    if([itemIdentifier isEqualToString: NSToolbarFlexibleSpaceItemIdentifier]){
    };
    return tbi;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
    return @[
    ];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar{
    return @[
        @"File Tree",
        @"Edit",
        @"New Doc",
    ];
}
- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return @[
        @"File Tree",
        @"Edit",
        @"New Doc",
    ];
}
- (NSSet<NSToolbarItemIdentifier> *)toolbarImmovableItemIdentifiers:(NSToolbar *)toolbar {
    return NSSet.set;
}

- (BOOL)toolbar:(NSToolbar *)toolbar itemIdentifier:(NSToolbarItemIdentifier)itemIdentifier canBeInsertedAtIndex:(NSInteger)index {
    return YES;
}

- (void)toolbarWillAddItem:(NSNotification *)notification {
}

- (void)toolbarDidRemoveItem:(NSNotification *)notification {
}
-(instancetype)initWithDoc:(DndDocument*)doc{
    self = [super init];
    self.doc = doc;
    NSScreen* screen = NSScreen.mainScreen;
    NSRect screenrect;
    if(screen)
        screenrect = screen.visibleFrame;
    else
        screenrect = NSMakeRect(0, 0, 1400, 800);

    NSRect webrect = screenrect;
    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    handler = [[DndUrlHandler alloc] init];
    handler.controller = self;
    [config.preferences setValue:@YES forKey:@"developerExtrasEnabled"];
    [config setURLSchemeHandler:handler forURLScheme:DND_SCHEME];
    webview = [[WKWebView alloc] initWithFrame:webrect configuration:config];
    [webview setValue: @NO forKey: @"drawsBackground"]; // key-value coding hackery
    [webview loadHTMLString:@"" baseURL:self.this_dnd_url];
    webview.allowsMagnification = YES;
    webnavdel = [[WebNavDel alloc] init];
    webnavdel.controller = self;
    webview.navigationDelegate = webnavdel;

    webview.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    webview.allowsBackForwardNavigationGestures = YES;
    webview.UIDelegate = self;
    toolbar = [[NSToolbar alloc] init];
    toolbar.delegate = self;
    // toolbar.displayMode = NSToolbarDisplayModeIconOnly;

    self.view = webview;
    return self;
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
            if(object == NSNull.null)
                return;
            if([object isKindOfClass:NSString.class])
                scroll_resto_string = object;
        }];
}
-(void)recalc_html:(NSString*)string{
    if(dont_update)
        return;
    const char* source_text = string.UTF8String;
    LongString source = {
        .text = source_text,
        // this is so dumb. Is there an API to get the length of the utf-8 string?
        // Maybe I should be turning NSString into
        // NSData and then borrowing the buffer?
        .length = strlen(source_text),
    };
    // FIXME: don't do this synchronously
    // FIXME: where the fuck are you supposed to put this stuff.
    if(!self.doc->auto_recalc)
        return;
    LongString html = {};
    NSString* dir = self.doc.fileURL.URLByDeletingLastPathComponent.path;
    StringView base_dir = SV("");
    if(dir){
        const char* dir_text = dir.UTF8String;
        base_dir.text = dir_text;
        base_dir.length = strlen(dir_text);
    }
    NSString* filename = self.doc.fileURL.path.lastPathComponent;
    // uint64_t t0 = get_t();
    uint64_t flags = 0;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    flags |= DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP;
    if(self.doc->show_stats)
        flags |= DNDC_PRINT_STATS;
    // Disabled until I can figure out how to get wkwebview to invalidate
    // cached images.
    // flags |= DNDC_USE_DND_URL_SCHEME;
    if(self.doc->editor_controller){
        self.doc->editor_controller->error_text.editable = YES;
        [self.doc->editor_controller->error_text.textStorage.mutableString setString:@""];
    }
    BOOL show_errors = !!self.doc->editor_controller && self.doc->show_errors;
    int err = run_the_dndc(OUTPUT_HTML, flags, base_dir, LS_to_SV(source), ns_borrow_sv(filename), &html, BASE64CACHE, TEXTCACHE, show_errors?gdndc_error_func:NULL, show_errors?(__bridge void*)self.doc->editor_controller->error_text:NULL, cache_watch_files, NULL, gdndc_ast_func, (__bridge void*)self.doc, (WorkerThread*)B64WORKER, LS(""));
    if(self.doc->editor_controller){
        self.doc->editor_controller->error_text.editable = NO;
    }
    // uint64_t t1 = get_t();
    if(err) return;

    PushDiagnostic();
    SuppressCastQual();
    NSData* htmldata = [NSData dataWithBytesNoCopy:(void*)html.text length:html.length+1 freeWhenDone:YES];
    PopDiagnostic();
    NSURL* url = self.this_dnd_url;
    [webview loadData:htmldata MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:url];
    // uint64_t t2 = get_t();
}

-(NSURL*) this_dnd_url {
    if(!self.doc.fileURL){
        return [NSURL URLWithString: DND_SCHEME_HOST "/nil.dnd"];
    }
    NSString* stringurl = [DND_SCHEME_HOST stringByAppendingString:[self.doc.fileURL.path stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet.URLPathAllowedCharacterSet]];
    NSURL* url = [NSURL URLWithString:stringurl];
    return url;
}

-(void)loadView {
    NSScreen* screen = NSScreen.mainScreen;
    NSRect screenrect;
    if(screen){
        screenrect = screen.visibleFrame;
    }
    else{
        screenrect = NSMakeRect(0, 0, 1400, 800);
    }
    self.view = [[NSView alloc] initWithFrame: screenrect];
}

- (void)webView:(WKWebView *)webView
        runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt
        defaultText:(NSString *_Nullable)defaultText
        initiatedByFrame:(WKFrameInfo *)frame
        completionHandler:(void (^)(NSString *result))completionHandler
{
    NSAlert* alert = [[NSAlert alloc] init];
    // [alert setTitle:@"Room"];
    alert.messageText = prompt;
    [alert addButtonWithTitle:@"Submit"];
    [alert addButtonWithTitle:@"Cancel"];
    alert.icon = nil;
    NSRect input_frame = NSMakeRect(0, 0, 300, 24);
    NSTextField* text_field = [[NSTextField alloc] initWithFrame:input_frame];
    alert.accessoryView = text_field;
    alert.window.initialFirstResponder = text_field;
    NSModalResponse response = [alert runModal];
    if(response == NSAlertFirstButtonReturn){
        completionHandler(text_field.stringValue);
    }
    else {
        completionHandler(@"");
    }
}
@end

@implementation DndWindowController : NSWindowController
-(NSString*)windowTitleForDocumentDisplayName:(NSString*)displayName{
    NSString* result = ((DndDocument*)self.document)->doc_title;
    if(!result)
        return displayName;
    return result;
}

-(void)keyDown:(NSEvent*) event{
    if(event.modifierFlags & NSEventModifierFlagCommand){
        NSInteger num = event.characters.integerValue;
        if(num){
            num -= 1;
           NSArray<NSWindow*>* tabs =  get_main_window().tabbedWindows;
           if(tabs.count > num){
                NSWindow* tab = tabs[num];
                [tab makeKeyAndOrderFront:nil];
                if(tab.childWindows && tab.childWindows.count)
                    [tab.childWindows[0] makeKeyAndOrderFront:nil];
                return;
           }
        }
    }
    [super keyDown:event];
}
@end
@implementation DndEditViewController{
}

#ifdef DNDC_DEVELOPER
#define BUTTON_LABELS(x) \
    x("Auto-Apply") \
    x("Coord Helper") \
    x("Show Errors") \
    x("Show Stats") \

#else

#define BUTTON_LABELS(x) \
    x("Auto-Apply") \
    x("Coord Helper") \
    x("Show Errors") \

#endif


enum DndEditViewButtonTags {
    DND_AUTO_APPLY_CHANGES_TAG = 1,
    DND_COORD_HELPER_TAG = 2,
    DND_SHOW_ERRORS_TAG = 3,
#ifdef DNDC_DEVELOPER
    DND_SHOW_STATS_TAG = 4,
#endif
};
-(void)scroll_to_line:(int)line column:(int)column{
    // this is ridiculous
    NSUInteger i = 0;
    int lineno = 1;
    {
        NSString* s = self.text.string;
        for(i = 0; lineno < line; i++){
            unichar c = [s characterAtIndex:i];
            if(c == u'\n')
                lineno++;
        }
    }
    [self.text scrollRangeToVisible:NSMakeRange(i, 1+column)];
    self.text.selectedRange = NSMakeRange(i+column+1, 0);
}
-(void)button_click:(id)a{
    // Actually button or menu item, but both respond to tag and state
    if([a isKindOfClass:NSButton.class]){
        NSButton* b = a;
        NSControlStateValue state = b.state;
        switch(b.tag){
            case DND_AUTO_APPLY_CHANGES_TAG:{
                if(state == NSControlStateValueOn){
                    self.doc->auto_recalc = YES;
                    [self.doc recalc_html];
                }
                else {
                    self.doc->auto_recalc = NO;
                }
                break;
            }
            case DND_COORD_HELPER_TAG:{
                if(state == NSControlStateValueOn){
                    self.doc->coord_helper = YES;
                    [self.doc recalc_html];
                }
                else {
                    self.doc->coord_helper = NO;
                    [self.doc recalc_html];
                }
                break;
            }
            case DND_SHOW_ERRORS_TAG:{
                if(state == NSControlStateValueOn){
                    self.doc->show_errors = YES;
                    [self.doc recalc_html];
                }
                else {
                    self.doc->show_errors = NO;
                    [self.doc recalc_html];
                }
                break;
            }
            #ifdef DNDC_DEVELOPER
            case DND_SHOW_STATS_TAG:{
                if(state == NSControlStateValueOn){
                    self.doc->show_stats = YES;
                    [self.doc recalc_html];
                }
                else {
                    self.doc->show_stats = NO;
                }
                break;
            }
            #endif
            default: {
                NSString* title = b.title;
                NSLog(@"Unknown button title:%@", title);
            }
        }
    }
}
- (BOOL)textView:(NSTextView *)textView clickedOnLink:(id)link atIndex:(NSUInteger)charIndex{
    if([link isKindOfClass:DndLink.class]){
        DndLink* l = link;
        NSURL* real_url = [self.doc.fileURL.URLByDeletingLastPathComponent URLByAppendingPathComponent:l->filename];
        int line = l->line;
        int col = l->col;
        if([real_url isEqualTo:self.doc.fileURL]){
            [self.doc scroll_to_line:line column:col];
        }
        else {
            [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:real_url display:YES completionHandler:^(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error){
                (void)documentWasAlreadyOpen;
                if(!error){
                    DndDocument* doc = (DndDocument*)document;
                    [doc scroll_to_line:line column: col];
                    [doc recalc_html];
                }
            }];
        }
        return YES;
    }
    (void)charIndex;
    (void)textView;
    return NO;
}
-(instancetype)initWithDoc:(DndDocument*)doc withRect:(NSRect)textrect{
    self = [super init];
    if(!self) return nil;
    self.doc = doc;

    highlighter = [[DndHighlighter alloc] init];
    highlighter.doc = doc;
    self.text = doc->text;
    self.text.textStorage.delegate = highlighter;

    editor_container = [[NSSplitView alloc] initWithFrame:textrect];
    editor_container.vertical = NO;

    error_text = [[NSTextView alloc] init];
    error_text.textStorage.font = EDITOR_FONT;
    error_text.editable = NO;
    error_text.usesAdaptiveColorMappingForDarkAppearance = YES;
    error_text.delegate = self;

    scrollview = [[NSScrollView alloc] initWithFrame:textrect];
    // scrollview.borderType = NSNoBorder;
    scrollview.hasVerticalScroller = YES;
    scrollview.hasHorizontalScroller = NO;
    scrollview.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin;
    scrollview.documentView = self.text;
    scrollview.findBarPosition = NSScrollViewFindBarPositionAboveContent;

    [editor_container addSubview:scrollview];
    self->toolbar = [[NSToolbar alloc] initWithIdentifier:@""];
    self->toolbar.delegate = self;
    self->toolbar.displayMode = NSToolbarDisplayModeIconOnly;
    [editor_container addSubview:error_text];
    [editor_container adjustSubviews];
    self.view = editor_container;
    return self;
}
- (nullable NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    // LOGIT(itemIdentifier);
    NSToolbarItem* tbi = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
    enum DndEditViewButtonTags tags[] = {
        DND_AUTO_APPLY_CHANGES_TAG,
        // DND_READ_ONLY_TAG,
        DND_COORD_HELPER_TAG,
        DND_SHOW_ERRORS_TAG,
#ifdef DNDC_DEVELOPER
        DND_SHOW_STATS_TAG,
#endif
    };
    NSString* button_labels[] = {
        #define X(a) @a,
        BUTTON_LABELS(X)
        #undef X
    };
    BOOL button_states[] = {
        self.doc->auto_recalc,
        // !self.doc->text.editable,
        self.doc->coord_helper,
        self.doc->show_errors,
#ifdef DNDC_DEVELOPER
        self.doc->show_stats,
#endif
    };
    _Static_assert(arrlen(button_states)==arrlen(button_labels), "");
    _Static_assert(arrlen(button_states)==arrlen(tags), "");
    for(size_t i = 0; i < arrlen(button_labels); i++){
        if(![itemIdentifier isEqualToString:button_labels[i]])
            continue;
        NSButton* button = [NSButton checkboxWithTitle:button_labels[i] target:self action:@selector(button_click:)];
        button.state = button_states[i]?NSControlStateValueOn:NSControlStateValueOff;
        button.tag = tags[i];
        tbi.view = button;
        tbi.label = button_labels[i];
        tbi.tag = tags[i];
    }
    return tbi;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
    return @[];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar{
    return @[
        #define X(a) @a,
        BUTTON_LABELS(X)
        #undef X
        NSToolbarShowFontsItemIdentifier,
    ];
}
- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return @[
        #define X(a) @a,
        BUTTON_LABELS(X)
        #undef X
        NSToolbarShowFontsItemIdentifier,
    ];
}
- (NSSet<NSToolbarItemIdentifier> *)toolbarImmovableItemIdentifiers:(NSToolbar *)toolbar {
    return NSSet.set;
}

- (BOOL)toolbar:(NSToolbar *)toolbar itemIdentifier:(NSToolbarItemIdentifier)itemIdentifier canBeInsertedAtIndex:(NSInteger)index {
    return YES;
}

- (void)toolbarWillAddItem:(NSNotification *)notification {
}

- (void)toolbarDidRemoveItem:(NSNotification *)notification {
}

-(void)insert_file:(id)sender{
    NSMenuItem* item = sender;
    NSOpenPanel* panel = NSOpenPanel.openPanel;
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    switch(item.tag){
        case GDND_INSERT_IMGLINKS:
        case GDND_INSERT_IMG:
            // The suggested fix is to use -allowedContentTypes, but
            // that is only available since 11.0 and I want to run
            // on 10.15.
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"
            panel.allowedFileTypes = @[@"png", @"jpg"];
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
            #pragma clang diagnostic pop
    }
    [panel beginWithCompletionHandler:^(NSInteger result){
        if(result == NSModalResponseOK){
            assert(panel.URL);
            NSURL* url = panel.URL;
            NSSize size = {};
            if(item.tag == GDND_INSERT_IMGLINKS){
                NSImage* img = [[NSImage alloc] initByReferencingURL:url];
                size.height = [img representations][0].pixelsHigh;
                size.width = [img representations][0].pixelsWide;
            }
            NSString* path;
            path = panel.URL.path;
            if(self.doc.fileURL){
                NSArray<NSString*>* chosen_components = panel.URL.pathComponents;
                NSArray<NSString*>* doc_components = self.doc.fileURL.pathComponents;
                if(doc_components.count > chosen_components.count){
                    goto have_path;
                }
                NSUInteger shorter = doc_components.count;
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
            [self.text insert_file_block:path tag:item.tag size:size];
        }
    }];
}
-(void)format_dnd:(id)sender {
    CGFloat before = self->scrollview.lineScroll;
    NSString *string = self.text.string;
    const char* source_text = string.UTF8String;
    // uint64_t t1 = get_t();
    LongString html = {};
    size_t len = strlen(source_text);
    error_text.editable = YES;
    error_text.textStorage.mutableString.string = @"";
    int err = dndc_format((StringView){len, source_text}, &html, gdndc_error_func, (__bridge void*)error_text);
    error_text.editable = NO;
    if(err){
        return;
    }
    PushDiagnostic();
    SuppressCastQual();
    NSData* htmldata = [NSData dataWithBytesNoCopy:(void*)html.text length:html.length freeWhenDone:YES];
    PopDiagnostic();
    NSString* str = [[NSString alloc] initWithData:htmldata encoding:NSUTF8StringEncoding];
    if(!str)
        return;
    [self.text insertText:str replacementRange:NSMakeRange(0, self.text.textStorage.length)];
    self->scrollview.lineScroll = before;
}

@end

@implementation DndAppDelegate{
    DndFontDelegate* fontdel;
    NSWindow* licenses_window;
}

#if 0
-(IBAction)newWindowForTab:(id)sender{
    NSLog(@"%@", NSThread.callStackSymbols);
    LOGIT(sender);
    [(id)NSDocumentController.sharedDocumentController newWindowForTab:sender];
    // return nil;
}
#endif
-(void)show_file_tree:(nullable id) sender{
    if(!filetree_window){
        NSWindow* w = get_main_window();
        if(w){
            NSViewController* vc = w.contentViewController;
            if([vc isKindOfClass:DndWebViewController.class]){
                DndWebViewController* wc = (DndWebViewController*)vc;
                set_filetree_window_url(wc.doc.fileURL.URLByDeletingLastPathComponent);
                return;
            }
        }
        [self openFolder:sender];
        return;
    }
    if(filetree_window.visible){
        [filetree_window close];
        return;
    }
    [filetree_window makeKeyAndOrderFront:nil];
}
-(void)openFolder:(nullable id) sender {
    if(filetree_window){
        [filetree_window close];
    }
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    [panel beginWithCompletionHandler:^(NSInteger result){
        if(result == NSModalResponseOK){
            set_filetree_window_url(panel.URL);
        }
    }];
}
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app{
    (void)app;
    return YES;
}
-(void)applicationWillFinishLaunching:(NSNotification *)notification{
    do_syntax_colors();
    do_menus();
}
- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender{
    NSDocumentController* controller = NSDocumentController.sharedDocumentController;
#if 1 && defined(DNDC_DEVELOPER)
    [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:[NSURL fileURLWithPath:@"/Users/drpriver/Documents/Dungeons/BarrowMaze/the-forgotten-antechamber.dnd"] display:YES completionHandler:^(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error){
        (void)document;
        (void)documentWasAlreadyOpen;
        (void)error;

    }];
    (void)controller;
#else
    // Dude, there's no way this is how you are supposed to do this.
    static BOOL opened = NO;
    if(!opened){
        opened = YES;
        [controller openDocument:nil];
    }
    return NO;
#endif
    return NO;
}

-(void)purge_file_caches:(id)sender{
    dndc_filecache_clear(BASE64CACHE);
    dndc_filecache_clear(TEXTCACHE);
}

#if 0
-(void)change_font{
    NSFontManager* mgr = NSFontManager.sharedFontManager;
    LOGIT([mgr selectedFont]);
}
#endif
-(void)applicationDidFinishLaunching:(NSNotification *)notification{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSApp.applicationIconImage = appimage;
    NSFontPanel* panel = NSFontPanel.sharedFontPanel;
    fontdel = [[DndFontDelegate alloc] init];
    panel.delegate = fontdel;

    [panel setPanelFont:EDITOR_FONT isMultiple:NO];
    NSFontManager* mgr = NSFontManager.sharedFontManager;
    [mgr setTarget:fontdel];
    // [mgr setAction:@selector(change_font)];

    NSRect rect = NSMakeRect(600, 600, 600, 600);
    licenses_window = [[NSWindow alloc]
        initWithContentRect: rect
        styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
        backing: NSBackingStoreBuffered
        defer: YES];
    licenses_window.releasedWhenClosed = NO;
    NSTextView* creditstext;
    creditstext = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600)];
    creditstext.string = @DNDC_OPEN_SOURCE_CREDITS;
    creditstext.usesAdaptiveColorMappingForDarkAppearance = YES;
    creditstext.editable = NO;
    creditstext.textStorage.font = [NSFont fontWithName:@"SF Mono" size:11];
    NSScrollView* credits;
    credits = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600)];
    credits.hasVerticalScroller = YES;
    credits.hasHorizontalScroller = NO;
    credits.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    credits.documentView = creditstext;
    licenses_window.contentView = credits;
    licenses_window.title = @"Open Source Licenses";
#if 0
    // this code is totally unnecessary as the webviews are aware of dark mode - duh!
    // I am leaving it here in case I need it for something else.
    static id o;
    o = [NSDistributedNotificationCenter.defaultCenter addObserverForName:@"AppleInterfaceThemeChangedNotification" object:nil queue:nil usingBlock:^(NSNotification* note){
        NSArray<NSWindow*> windows = [NSApp windows];
        for(NSWindow* win in windows){
            NSViewController* vc = win.contentViewController;
            if([vc isKindOfClass:DndViewController.class]){
             [(DndViewController*)vc refresh];
            }
        }
    }];
#endif
}

#if 0
-(void)did_dark_mode:(nullable id) sender {
}
#endif
-(void)show_licenses:(nullable id) sender{
    [licenses_window makeKeyAndOrderFront:self];
}


@end

@implementation DndFontDelegate
// this shit is deprecated apparently.
// but uh. Whatever.
-(void)changeFont:(nullable id)sender{
    NSFont* font = [sender convertFont:EDITOR_FONT];
    if(!font)
        return;
    EDITOR_FONT = font;
    NSDocumentController* controller = NSDocumentController.sharedDocumentController;
    NSArray<DndDocument*>* documents = controller.documents;
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
    NSFont* font = [NSFont fontWithName:@"SF Mono" size:11];
    if(!font) font = [NSFont fontWithName:@"Courier" size:11];
    if(!font) font = [NSFont fontWithName:@"Menlo" size:11];
    EDITOR_FONT = font;
    BASE64CACHE = dndc_create_filecache();
    TEXTCACHE = dndc_create_filecache();
    B64WORKER = dndc_worker_thread_create();
    NSApplication* app = NSApplication.sharedApplication;
    DndAppDelegate* appDelegate = [DndAppDelegate.alloc init];
    app.delegate = appDelegate;
    ptrdiff_t icon_size = _app_icon_end - _app_icon;
    NSData* imagedata = [NSData dataWithBytesNoCopy:(void*)_app_icon length:icon_size freeWhenDone:NO];
    PushDiagnostic();
    #pragma clang diagnostic ignored "-Wnullable-to-nonnull-conversion"
    appimage = [[NSImage alloc] initWithData:imagedata];
    PopDiagnostic();
    app.applicationIconImage = appimage;
    return NSApplicationMain(argc, argv);
}

static
void
do_syntax_colors(void){
    for(int i = 0; i < DNDC_SYNTAX_MAX; i++){
        SYNTAX_COLORS[i] = NSColor.textColor; // set up backup
    }
    SYNTAX_COLORS[DNDC_SYNTAX_DOUBLE_COLON]       = NSColor.lightGrayColor;
    SYNTAX_COLORS[DNDC_SYNTAX_HEADER]             = NSColor.systemBlueColor;
    SYNTAX_COLORS[DNDC_SYNTAX_NODE_TYPE]          = NSColor.darkGrayColor;
    SYNTAX_COLORS[DNDC_SYNTAX_ATTRIBUTE]          = NSColor.systemBrownColor;
    SYNTAX_COLORS[DNDC_SYNTAX_DIRECTIVE]          = NSColor.systemPurpleColor;
    SYNTAX_COLORS[DNDC_SYNTAX_ATTRIBUTE_ARGUMENT] = NSColor.systemBrownColor;
    SYNTAX_COLORS[DNDC_SYNTAX_CLASS]              = NSColor.systemGrayColor;
    SYNTAX_COLORS[DNDC_SYNTAX_RAW_STRING]         = NSColor.systemPinkColor; // currently unused

    NSColor* blendedteal = [NSColor.systemTealColor blendedColorWithFraction:0.5 ofColor:NSColor.blackColor];
    NSColor* blendedgreen = [NSColor.systemGreenColor blendedColorWithFraction:0.5 ofColor:NSColor.blackColor];
    NSColor* blendedorange = [NSColor.systemOrangeColor blendedColorWithFraction:0.5 ofColor:NSColor.blackColor];
    // javascript colors
    SYNTAX_COLORS[DNDC_SYNTAX_JS_COMMENT]       = NSColor.systemGrayColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_STRING]        = blendedgreen;//NSColor.systemGreenColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_REGEX]         = NSColor.systemRedColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_NUMBER]        = blendedgreen;//NSColor.systemGreenColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_KEYWORD]       = blendedteal;//NSColor.systemTealColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_KEYWORD_VALUE] = blendedgreen;//NSColor.systemGreenColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_VAR]           = blendedteal;//NSColor.systemTealColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_IDENTIFIER]    = NSColor.textColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_BUILTIN]       = blendedorange;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_NODETYPE]      = NSColor.systemOrangeColor;
    SYNTAX_COLORS[DNDC_SYNTAX_JS_BRACE]         = NSColor.headerTextColor;
}

static
void
do_menus(void){
    NSMenu *mainMenu = [[NSMenu alloc] init];
    // Create the main menu bar
    NSApp.mainMenu = mainMenu;

    {
        // Create the application menu
        NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

        // Add menu items
        NSString *title = [[@"About " stringByAppendingString:APPNAME] stringByAppendingString:@"…"];
        [menu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

        [menu addItem:NSMenuItem.separatorItem];

        [menu addItemWithTitle:@"Preferences…" action:nil keyEquivalent:@","];

        [menu addItem:NSMenuItem.separatorItem];

        NSMenu* serviceMenu = [[NSMenu alloc] initWithTitle:@""];
        NSMenuItem* menu_item = [menu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
        menu_item.submenu = serviceMenu;

        [NSApp setServicesMenu:serviceMenu];

        [menu addItem:NSMenuItem.separatorItem];

        title = [@"Hide " stringByAppendingString:APPNAME];
        [menu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

        menu_item = [menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [menu_item setKeyEquivalentModifierMask:(NSEventModifierFlagOption|NSEventModifierFlagCommand)];

        [menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

        [menu addItem:NSMenuItem.separatorItem];

        title = [@"Quit " stringByAppendingString:APPNAME];
        [menu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

        menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        menu_item.submenu = menu;
        [NSApp.mainMenu addItem:menu_item];
    }

    // Create the File menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"File"];
        [menu addItemWithTitle:@"New Tab" action:@selector(newWindowForTab:) keyEquivalent:@"n"];
        [menu addItemWithTitle:@"New Tab" action:@selector(newWindowForTab:) keyEquivalent:@"t"];
        [menu addItemWithTitle:@"Open" action:@selector(openDocument:) keyEquivalent:@"o"];
        [menu addItemWithTitle:@"Open Folder" action:@selector(openFolder:) keyEquivalent:@"O"];
        [menu addItem:NSMenuItem.separatorItem];
        [menu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
        [menu addItemWithTitle:@"Save" action:@selector(saveDocument:) keyEquivalent:@"s"];
        [menu addItemWithTitle:@"Revert to Saved" action:@selector(revertDocumentToSaved:) keyEquivalent:@""];
        [menu addItem:NSMenuItem.separatorItem];
        [menu addItemWithTitle:@"Empty File Caches" action:@selector(purge_file_caches:) keyEquivalent:@""];
        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        menu_item.submenu = menu;
        [NSApp.mainMenu addItem:menu_item];
    }
    // Create the edit menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Edit"];
        [menu addItemWithTitle:@"Edit it" action:@selector(pop_out_editor:) keyEquivalent:@"e"];
        [menu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
        [menu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
        NSMenuItem* fontmi = [[NSMenuItem alloc] initWithTitle:@"Font" action:nil keyEquivalent:@""];
        NSFontManager *fontManager = NSFontManager.sharedFontManager;
        NSMenu *fontMenu = [fontManager fontMenu:YES];
        fontmi.submenu = fontMenu;
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
        menu_item.submenu = menu;
        [NSApp.mainMenu addItem:menu_item];
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

        menu_item.submenu = menu;
        [NSApp.mainMenu addItem:menu_item];
    }
    // Create the view menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"View"];
        [menu addItemWithTitle:@"File Tree" action:@selector(show_file_tree:) keyEquivalent:@"l"];
        [menu addItem:NSMenuItem.separatorItem];
        [menu addItemWithTitle:@"Refresh" action:@selector(refresh) keyEquivalent:@"r"];
        [menu addItemWithTitle:@"Scroll Into View" action:@selector(scroll_selection_into_view:) keyEquivalent:@"\r"];
        [menu addItem:NSMenuItem.separatorItem];
        [menu addItemWithTitle:@"Zoom Out" action:@selector(zoom_out:) keyEquivalent:@"-"];
        [menu addItemWithTitle:@"Zoom In" action:@selector(zoom_in:) keyEquivalent:@"+"];
        [menu addItemWithTitle:@"Actual Size" action:@selector(zoom_normal:) keyEquivalent:@"0"];

        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        menu_item.submenu = menu;
        [NSApp.mainMenu addItem:menu_item];
    }


    // Create the window menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Window"];

        [menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
        [menu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
        menu_item.submenu = menu;
        [NSApp.mainMenu addItem:menu_item];

        NSApp.windowsMenu = menu;
    }
    // Create the help menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Help"];

        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
        [menu addItemWithTitle:@"Open Source Licenses…" action:@selector(show_licenses:) keyEquivalent:@""];
        menu_item.submenu = menu;
        [NSApp.mainMenu addItem:menu_item];
        NSApp.helpMenu = menu;
    }
}

#pragma clang assume_nonnull end

#import "Dndc/dndc.c"
#import "Allocators/allocator.c"
#import "filewatchcache.m"
#import "filetree.m"
