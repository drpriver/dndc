#import <Cocoa/Cocoa.h>
#import "dndc_long_string.h"
#import "dndc_local_server.h"
#import "common_macros.h"
#import <string.h>
#define LOGIT(...) NSLog(@ "%d: " #__VA_ARGS__ "= %@", __LINE__, __VA_ARGS__)
// We only compile this with apple clang anyway, so using extensions is fine.
#define auto __auto_type

#if !__has_feature(objc_arc)
#error "ARC is off"
#endif
@interface Message : NSObject{
@public int type;
@public NSString* filename;
@public int line;
@public int col;
@public NSString* message;
}
@end

@implementation Message
@end

@interface DndBrAppDelegate : NSObject<NSApplicationDelegate>
@end
@interface DndViewController: NSViewController <NSTextFieldDelegate, NSBrowserDelegate>
-(void)log_mess:(int)type fn:(NSString*)filename line:(int)l column:(int)col mess:(NSString*)message;
-(void)log_mess_v:(Message*)value;
@end


@interface FileSystemNode : NSObject // <NSToolbarDelegate>

-(id)initWithURL:(NSURL *)url NS_DESIGNATED_INITIALIZER;
@property(readonly) NSURL *URL;
@property(readonly, copy) NSString *displayName;
@property(readonly, strong) NSImage *icon;
@property(readonly, strong) NSArray *children;
@property(readonly) BOOL isDirectory;
@property(readonly) BOOL isPackage;
@property(readonly, strong) NSColor *labelColor;
-(void)invalidateChildren;
@end

@interface FileSystemNode ()
@property (strong) NSURL *URL;
@property (assign) BOOL childrenDirty;
@property (strong) NSMutableDictionary *internalChildren;
@end


@implementation FileSystemNode
// @dynamic displayName, children, isDirectory, icon, labelColor;

- (instancetype)init {
    NSAssert(NO, @"Invalid use of init; use initWithURL to create FileSystemNode");
    return [self init];
}

- (id)initWithURL:(NSURL *)url {
    self = [super init];
    if (self != nil) {
        _URL = url;
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ - %@", super.description, self.URL];
}

- (NSString *)displayName {
    NSString *displayName = @"";
    NSError *error = nil;
    BOOL success = [self.URL getResourceValue:&displayName forKey:NSURLLocalizedNameKey error:&error];
    // if we got a no value for the localized name, we will try the non-localized name
    if (success && displayName.length > 0) {
        [self.URL getResourceValue:&displayName forKey:NSURLNameKey error:&error];
    }
    else {
        // can't find resource value for the display name, use the localizedDescription as last resort
        return error.localizedDescription;
    }
    return displayName;
}

- (NSImage *)icon {
    NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile:[self.URL path]];
    icon.size = NSMakeSize(16, 16);
    return icon;

}

- (BOOL)isDirectory {
    id value = nil;
    [self.URL getResourceValue:&value forKey:NSURLIsDirectoryKey error:nil];
    return [value boolValue];
}

- (BOOL)isPackage {
    id value = nil;
    [self.URL getResourceValue:&value forKey:NSURLIsPackageKey error:nil];
    return [value boolValue];
}

- (NSColor *)labelColor {
    id value = nil;
    [self.URL getResourceValue:&value forKey:NSURLLabelColorKey error:nil];
    return value;
}

- (NSArray *)children {
    if (self.internalChildren == nil || self.childrenDirty) {
        // This logic keeps the same pointers around, if possible.
        NSMutableDictionary *newChildren = [NSMutableDictionary new];

        NSString *parentPath = [self.URL path];
        NSArray *contentsAtPath = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:parentPath error:nil];

        if(contentsAtPath){	// We don't deal with the error
            for (NSString *filename in contentsAtPath) {
                if([filename characterAtIndex:0] == '.') continue;
                if([filename characterAtIndex:0] == '_') continue;
                // Use the filename as a key and see if it was around and reuse it, if possible
                if (self.internalChildren != nil) {
                    FileSystemNode *oldChild = [self.internalChildren objectForKey:filename];
                    if (oldChild != nil) {
                        [newChildren setObject:oldChild forKey:filename];
                        continue;
                    }
                }
                // We didn't find it, add a new one
                NSString *fullPath = [parentPath stringByAppendingPathComponent:filename];
                NSURL *childURL = [NSURL fileURLWithPath:fullPath];
                if (childURL != nil) {
                    // Wrap the child url with our node
                    FileSystemNode *node = [[FileSystemNode alloc] initWithURL:childURL];
                    if((node.isDirectory && ![filename pathExtension].length) || [[filename pathExtension] isEqualToString:@"dnd"])
                        [newChildren setObject:node forKey:filename];
                }
            }
        }

        self.internalChildren = newChildren;
        self.childrenDirty = NO;
    }

    NSArray *result = [self.internalChildren allValues];

    // Sort the children by the display name and return it
    result = [result sortedArrayUsingComparator:^(id obj1, id obj2) {
        NSString *objName = [obj1 displayName];
        NSString *obj2Name = [obj2 displayName];
        NSComparisonResult sortedResult = [objName compare:obj2Name options:NSNumericSearch | NSCaseInsensitiveSearch | NSWidthInsensitiveSearch | NSForcedOrderingSearch range:NSMakeRange(0, [objName length]) locale:[NSLocale currentLocale]];
        return sortedResult;
    }];
    return result;
}

- (void)invalidateChildren {
    _childrenDirty = YES;
    for (FileSystemNode *child in [self.internalChildren allValues]) {
        [child invalidateChildren];
    }
}

@end



static
void
logfunc(void*_Nullable p, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    if(!p) return;
    // dndc_stderr_error_func(p, type, filename, filename_len, line, col, message, message_len);
    DndViewController* controller = (__bridge DndViewController*)p;
    NSString* fn = [[NSString alloc] initWithBytes:filename length:filename_len encoding:NSUTF8StringEncoding];
    assert(fn);
    NSString* mess = [[NSString alloc] initWithBytes:message length:message_len encoding:NSUTF8StringEncoding];
    assert(mess);
    Message* m = [[Message alloc] init];
    m->type = type;
    m->filename = fn;
    m->line = line;
    m->col = col;
    m->message = mess;

    [controller performSelectorOnMainThread:@selector(log_mess_v:) withObject:m waitUntilDone:NO];
}

//
// Setup menus without needing a nib (xib? whatever).
//
static void do_menus(void);

// The app's image. We embed the png into the binary and decode it at startup.
static NSImage* appimage;
static NSWindow* the_window;
// Name of the app
static NSString* APPNAME = @"Gdndc";
extern char _app_icon[];
extern char _app_icon_end[];
asm(".global __app_icon\n"
    ".global __app_icon_end\n"
    "__app_icon:\n"
#ifdef APP_ICON_PATH
    ".incbin \"" APP_ICON_PATH "\"\n"
#else
    ".incbin \"Platform/MacOS/dndbr_app_icon.png\"\n"
#endif

    "__app_icon_end:\n");

@implementation DndBrAppDelegate {
    NSStatusItem* si;
}
-(void)applicationWillFinishLaunching:(NSNotification *)notification{
    do_menus();
}
-(void)show_app:(id _Nullable) sender{
    [NSApp activateIgnoringOtherApps:YES];
    [the_window makeKeyAndOrderFront:nil];
}
-(void)applicationDidFinishLaunching:(NSNotification *)notification{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    // [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp activateIgnoringOtherApps:YES];
    NSApp.applicationIconImage = appimage;

    NSRect rect = NSMakeRect(200, 200, 800, 800);
    the_window = [[NSWindow alloc]
        initWithContentRect: rect
        styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
        backing: NSBackingStoreBuffered
        defer: YES];
    the_window.title = @"DndBr";
    the_window.releasedWhenClosed = NO;
    the_window.autorecalculatesKeyViewLoop = NO;
    the_window.contentViewController = [[DndViewController alloc] init];
    [the_window makeKeyAndOrderFront:nil];
}
-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender{
    return NO;
}
- (void)applicationDidBecomeActive:(NSNotification *)notification{
    [the_window makeKeyAndOrderFront:nil];
}
@end

@interface BrowserCell : NSBrowserCell
@end

@implementation BrowserCell
@end

@interface MyBrowser : NSBrowser
@end

@implementation MyBrowser
-(BOOL)canBecomeKeyView{
    return YES;
}
- (void)keyDown:(NSEvent *)event{
    if(event.type == NSEventTypeKeyDown && event.keyCode == 36){
        [self doDoubleClick:self];
    }
    else {
        [super keyDown:event];
    }
}
@end

@implementation DndViewController{
    NSTextField* text;
    NSButton* start_stop;
    NSButton* pick;
    NSTextView* log;
    NSScrollView* logscroll;
    NSStackView* controls;
    NSSplitView* split_view;
    NSBrowser* browser;
    NSThread* serverthread;
    BOOL serving;
    int port;
    int socket;
    FileSystemNode* rootNode;
}
-(void)viewDidAppear{
    [self->browser selectRow:0 inColumn:0];
    self->text.nextKeyView = self->browser;
    self->browser.nextKeyView = self->text;
}
-(instancetype)init{
    self = [super init];
    if(!self) return self;
    self->rootNode = nil;
    self->text = [NSTextField textFieldWithString:[[NSFileManager alloc]init].currentDirectoryPath];
    self->text.placeholderString = @"Directory to serve from...";
    self->text.delegate = self;
    self->start_stop = [NSButton buttonWithTitle:@"Start" target:self action:@selector(start_stop_server:)];
    NSImage* img = [NSImage imageNamed:NSImageNameFolder];
    self->pick = [NSButton buttonWithImage:img target:self action:@selector(pick_folder:)];
    self->log = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 800, 200)];
    self->log.editable = NO;
    self->log.font = [NSFont fontWithName:@"SF Mono" size:14];
    self->log.usesAdaptiveColorMappingForDarkAppearance = YES;
    self->logscroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 800, 200)];
    self->logscroll.hasVerticalScroller = YES;
    self->logscroll.hasHorizontalScroller = NO;
    self->logscroll.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin;
    self->logscroll.documentView = self->log;
    self->controls = [[NSStackView alloc] initWithFrame:NSMakeRect(0, 0, 800, 80)];
    [self->controls addView:self->text inGravity:NSStackViewGravityLeading];
    [self->controls addView:self->pick inGravity:NSStackViewGravityLeading];
    [self->controls addView:self->start_stop inGravity:NSStackViewGravityLeading];
    self->controls.edgeInsets = NSEdgeInsetsMake(20, 20, 20, 20);
    self->browser = [[MyBrowser alloc] initWithFrame:NSMakeRect(0, 100, 800, 520)];
    self->browser.doubleAction = @selector(open_page:);
    self->browser.maxVisibleColumns = 2;
    self->browser.delegate = self;
    self->browser.cellClass = [BrowserCell class];
    self->split_view = [[NSSplitView alloc] initWithFrame:NSMakeRect(0, 0, 800, 800)];
    self->split_view.vertical = NO;
    [self->split_view addSubview:self->controls];
    [self->split_view addSubview:self->browser];
    [self->split_view addSubview:self->logscroll];
    [self->split_view adjustSubviews];
    self.view = self->split_view;
    self->serving = NO;
    self->text.editable = YES;
    self->pick.enabled = YES;
    self->port = 0;
    return self;
}

// NSBrowserDelegate

-(id)rootItemForBrowser:(NSBrowser *)browser {
    if(!self->serving) return nil;
    if (self->rootNode == nil) {
        self->rootNode = [[FileSystemNode alloc] initWithURL:[NSURL fileURLWithPath:self->text.stringValue]];
    }
    return self->rootNode;
}

-(NSInteger)browser:(NSBrowser *)browser numberOfChildrenOfItem:(id)item {
    if(!self->serving) return 0;
    FileSystemNode *node = (FileSystemNode *)item;
    return node.children.count;
}

-(id)browser:(NSBrowser *)browser child:(NSInteger)index ofItem:(id)item {
    if(!self->serving) return nil;
    FileSystemNode *node = (FileSystemNode *)item;
    return [node.children objectAtIndex:index];
}

- (BOOL)browser:(NSBrowser *)browser isLeafItem:(id)item {
    FileSystemNode *node = (FileSystemNode *)item;
    return !node.isDirectory || node.isPackage; // take into account packaged apps and documents
}

- (id)browser:(NSBrowser *)browser objectValueForItem:(id)item {
    FileSystemNode *node = (FileSystemNode *)item;
    return node.displayName;
}
-(void)browser:(NSBrowser *)sender willDisplayCell:(id)cell atRow:(NSInteger)row column:(NSInteger)column{
    // LOGIT(cell);
    BrowserCell* bc = cell;
    FileSystemNode* item = [sender itemAtRow:row inColumn:column];
    bc.image = item.icon;
}
// end NSBrowserDelegate

-(void)log_mess:(int)type fn:(NSString*)filename line:(int)l column:(int)col mess:(NSString*)message{
    NSString* type_str = @"?????";
    NSColor* color = [NSColor textColor];
    switch((enum DndcErrorMessageType)type){
        case DNDC_ERROR_MESSAGE:
            type_str = @"ERROR";
            color = [NSColor systemRedColor];
            break;
        case DNDC_WARNING_MESSAGE:
            type_str = @"WARN ";
            color = [NSColor systemYellowColor];
            break;
        case DNDC_NODELESS_MESSAGE:
            type_str = @"ERROR";
            color = [NSColor systemRedColor];
            break;
        case DNDC_STATISTIC_MESSAGE:
            type_str = @"INFO ";
            color = [NSColor systemCyanColor];
            break;
        case DNDC_DEBUG_MESSAGE:
            type_str = @"DEBUG";
            color = [NSColor systemMintColor];
            break;
    }
    NSMutableAttributedString* m = [NSMutableAttributedString localizedAttributedStringWithFormat:[[NSAttributedString alloc] initWithString:@"[%@] %@: %@\n"], type_str, filename, message];
    [m addAttribute:NSForegroundColorAttributeName value:color range: NSMakeRange(1, 5)];
    [m addAttribute:NSFontAttributeName value: [NSFont fontWithName:@"SF Mono" size:14] range:NSMakeRange(0, [m length])];
    [self->log.textStorage appendAttributedString:m];
    [self->log scrollToEndOfDocument:self];
    // [self->logscroll scrollToEndOfDocument:self->logscroll];
}
-(void)log_mess_v:(Message*)m{
    [self log_mess:m->type fn:m->filename line:m->line column:m->col mess:m->message];
}
-(void)server_did_start:(id _Nullable) sender{
    self->start_stop.title = @"Stop";
    self->serving = YES;
    self->text.editable = NO;
    self->text.enabled = NO;
    self->pick.enabled = NO;
    [the_window makeFirstResponder:self->browser];
    self->rootNode = nil;
    [self->browser loadColumnZero];
    [self->browser selectRow:0 inColumn:0];
}
-(void)start_server:(id _Nullable) sender{
    if(!self->serverthread){
        if(![self->rootNode.URL.path isEqualToString:self->text.stringValue]){
            [self->rootNode invalidateChildren];
            self->rootNode = nil;
            [self->browser loadColumnZero];
            [self->browser selectRow:0 inColumn:0];
        }
        [self->browser setNeedsDisplay:YES];
        const char* string = [[self->text stringValue] UTF8String];
        size_t len = strlen(string);
        if(!len) return;
        char* copy = malloc(len+1);
        memcpy(copy, string, len+1);
        self->serverthread = [[NSThread alloc] initWithBlock:^{
            [self performSelectorOnMainThread:@selector(server_did_start:) withObject:nil waitUntilDone:NO];
            int theport = self->port;
            DndServer* server = dnd_server_create(logfunc, (__bridge void*)self, &theport);
            if(!server){
                NSLog(@"No server!");
                return;
            }
            NSValue* d = [NSValue value:&theport withObjCType:@encode(int)];
            [self performSelectorOnMainThread:@selector(set_port:) withObject:d waitUntilDone:NO];
            // hack, but whatever
            self->socket = *(int*)server;
            dnd_server_serve(server, DNDC_ALLOW_BAD_LINKS|DNDC_DONT_INLINE_IMAGES, (LongString){len, copy});
            [self performSelectorOnMainThread:@selector(server_did_stop:) withObject:nil waitUntilDone:NO];
            free(copy);
        }];
        [self->serverthread start];
    }
}
-(void)set_port:(NSValue*)value{
    [value getValue:&self->port];
}
-(void)server_did_stop:(id _Nullable) sender{
    self->start_stop.title = @"Start";
    self->serving = NO;
    self->text.editable = YES;
    self->text.enabled = YES;
    [the_window makeFirstResponder:self->text];
    self->pick.enabled = YES;
    self->serverthread = nil;
    [self log_mess:DNDC_STATISTIC_MESSAGE fn:@"" line:-1 column:-1 mess:@"Successful Stop"];
    [self->rootNode invalidateChildren];
    self->rootNode = nil;
    [self->browser loadColumnZero];
}
-(void)stop_server:(id _Nullable) sender{
    if(!self->serving) return;
    // hack, but whatever
    close(self->socket);
}
-(void)start_stop_server:(id _Nullable) sender{
    if(self->serving)
        [self stop_server:sender];
    else
        [self start_server:sender];
}
-(void)open_page:(id _Nullable) sender{
    if(!self->serving) return;
    if(sender){
        NSIndexPath *indexPath = [browser selectionIndexPath];
        FileSystemNode *node = [browser itemAtIndexPath:indexPath];
        NSString* path = [node.URL path];
        // LOGIT(path);
        path = [path substringFromIndex:self->text.stringValue.length];
        // LOGIT(path);
        NSString* s = [NSString stringWithFormat:@"http://localhost:%d%@", self->port, path];
        // LOGIT(s);
        NSURL* u = [NSURL URLWithString:s];
        [[NSWorkspace sharedWorkspace] openURL:u];
        return;
    }
    NSString* s = [NSString stringWithFormat:@"http://localhost:%d", self->port];
    NSURL* u = [NSURL URLWithString:s];
    [[NSWorkspace sharedWorkspace] openURL:u];
}
-(void)pick_folder:(id _Nullable) sender{
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    [panel beginWithCompletionHandler:^(NSInteger result){
        if(result == NSModalResponseOK){
            NSURL* url = panel.URL;
            self->text.stringValue = [url path];
            if(![self->rootNode.URL.path isEqualToString:self->text.stringValue]){
                [self->rootNode invalidateChildren];
                self->rootNode = nil;
                [self->browser loadColumnZero];
                [self->browser selectRow:0 inColumn:0];
            }
        }
    }];
}
- (BOOL)control:(NSControl *)control textView:(NSTextView *)fieldEditor doCommandBySelector:(SEL)commandSelector{
    if (commandSelector == @selector(insertNewline:)) {
        if(!self->serving){
            [self start_server:nil];
        }
        else {
            [self open_page:self->browser];
        }
        return YES;
    }
    return NO;
}
@end

static
void
do_menus(void){
    NSMenu *mainMenu = [[NSMenu alloc] init];
    // Create the main menu bar
    [NSApp setMainMenu:mainMenu];

    {
        // Create the application menu
        NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

        // Add menu items
        NSString *title = [[@"About " stringByAppendingString:APPNAME] stringByAppendingString:@"…"];
        [menu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

        [menu addItem:[NSMenuItem separatorItem]];

        [menu addItemWithTitle:@"Preferences…" action:nil keyEquivalent:@","];

        [menu addItem:[NSMenuItem separatorItem]];

        NSMenu* serviceMenu = [[NSMenu alloc] initWithTitle:@""];
        NSMenuItem* menu_item = [menu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:serviceMenu];

        [NSApp setServicesMenu:serviceMenu];

        [menu addItem:[NSMenuItem separatorItem]];

        title = [@"Hide " stringByAppendingString:APPNAME];
        [menu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

        menu_item = [menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [menu_item setKeyEquivalentModifierMask:(NSEventModifierFlagOption|NSEventModifierFlagCommand)];

        [menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

        [menu addItem:[NSMenuItem separatorItem]];

        title = [@"Quit " stringByAppendingString:APPNAME];
        [menu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

        menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }

    // Create the File menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"File"];
        [menu addItemWithTitle:@"Open" action:@selector(open_folder:) keyEquivalent:@"o"];
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
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];

        NSMenuItem* menu_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menu_item setSubmenu:menu];
        [[NSApp mainMenu] addItem:menu_item];
    }
    // Create the view menu
    {
        NSMenu* menu = [[NSMenu alloc] initWithTitle:@"View"];
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
        [[NSApp mainMenu] addItem:menu_item];
        [NSApp setHelpMenu:menu];
    }
}


#pragma clang assume_nonnull begin
int
main(int argc, const char *_Null_unspecified *_Nonnull argv) {
    if(argc > 1){
        chdir(argv[1]);
    }
    NSApplication* app = [NSApplication sharedApplication];
    DndBrAppDelegate* appDelegate = [DndBrAppDelegate new];
    app.delegate = appDelegate;
    auto icon_size = _app_icon_end - _app_icon;
    NSData* imagedata = [NSData dataWithBytesNoCopy:(void*)_app_icon length:icon_size freeWhenDone:NO];
    PushDiagnostic();
    #pragma clang diagnostic ignored "-Wnullable-to-nonnull-conversion"
    appimage = [[NSImage alloc] initWithData:imagedata];
    PopDiagnostic();

    app.applicationIconImage = appimage;
    return NSApplicationMain(argc, argv);
}
#pragma clang assume_nonnull end
