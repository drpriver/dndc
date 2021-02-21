#import <Cocoa/Cocoa.h>
#import <Webkit/WebKit.h>
#include "measure_time.h"
#include "DndC/dndc.h"

NSString* testpath = @"/Users/drpriver/Documents/Dungeons/BarrowMaze/surface.dnd";
NSString * const kIndentPatternString = @"^(\\t|\\s)+";


@interface DndHighlighter : NSObject <NSTextStorageDelegate>

@end

@implementation DndHighlighter
- (void)textStorage:(NSTextStorage *)textStorage
  didProcessEditing:(NSTextStorageEditActions)editedMask
              range:(NSRange)editedRange
     changeInLength:(NSInteger)delta{
    NSString *string = textStorage.string;
    NSRange currentLineRange = [string lineRangeForRange:editedRange];
    [textStorage removeAttribute:NSForegroundColorAttributeName range:currentLineRange];
    NSUInteger rowstart = currentLineRange.location;
    BOOL saw_colon = NO;
    BOOL saw_double_colon = NO;
    BOOL all_spaces = YES;
    NSUInteger double_colon = 0;
    for (NSUInteger i = currentLineRange.location; i < currentLineRange.location+currentLineRange.length; i++) {
        unichar c = [string characterAtIndex:i];
        if(c == '\n'){
            if(saw_double_colon){
                [textStorage addAttribute:NSForegroundColorAttributeName value:[NSColor blueColor] range:NSMakeRange(rowstart, double_colon-rowstart)];
                [textStorage addAttribute:NSForegroundColorAttributeName value:[NSColor lightGrayColor] range:NSMakeRange(double_colon, i-double_colon)];
                saw_double_colon = NO;
                saw_colon = NO;
            }
            all_spaces = YES;
            rowstart = i;
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
    auto thing = [NSApp keyWindow].contentViewController;
    [thing performSelector:@selector(recalc_html:) withObject:nil];
}
@end

@interface DndTextView : NSTextView {
NSRegularExpression* indent_pattern;
}
@end

@implementation DndTextView

- (DndTextView*) initWithFrame:(NSRect)textrect{
    self = [super initWithFrame:textrect];
    self->indent_pattern = [[NSRegularExpression alloc] initWithPattern:kIndentPatternString options:0 error:nil];
    self.usesAdaptiveColorMappingForDarkAppearance = NO;
    self.automaticTextCompletionEnabled = NO;
    self.automaticLinkDetectionEnabled = NO;
    self.automaticSpellingCorrectionEnabled = NO;
    self.automaticDashSubstitutionEnabled = NO;
    self.automaticQuoteSubstitutionEnabled = NO;
    self.automaticDataDetectionEnabled = NO;
    self.font=[NSFont fontWithName:@"Menlo" size:11];
    self.textColor= NSColor.blackColor;
    self.backgroundColor= NSColor.whiteColor;
    self.insertionPointColor= NSColor.blackColor;
    self.usesFindBar = YES;
    self.incrementalSearchingEnabled = YES;
    NSColor *selectedbgcolor = [NSColor colorWithCalibratedRed:0xbb/255. green:0xd5/255. blue:0xfc/255. alpha:1.0];
    self.selectedTextAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
        selectedbgcolor, NSBackgroundColorAttributeName,
        [NSColor blackColor], NSForegroundColorAttributeName,
        nil];
    self.allowsUndo = YES;
    return self;
}

- (void) insertNewline:(id)sender {
    NSRange currentLineRange = [self.string lineRangeForRange:self.selectedRange];
    NSString* currentLine = [self.string substringWithRange:currentLineRange];
    [super insertNewline:sender];
    NSTextCheckingResult* indent_matched = [indent_pattern firstMatchInString:currentLine options:0 range:NSMakeRange(0, currentLine.length)];
    if (indent_matched) {
        NSString *indent = [currentLine substringWithRange:indent_matched.range];
        [self insertText:indent replacementRange:self.selectedRange];
    }
}
- (void)insertTab:(id)sender{
 [self insertText: @"  " replacementRange:self.selectedRange];
}

@end

@interface ViewController : NSViewController {
    DndTextView* text;
    NSScrollView* scrollview;
    WKWebView* webview;
    DndHighlighter* highlighter;
}
@end

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    NSRect textrect = {.origin={1400-550,0}, .size={550,800}};
    text = [[DndTextView alloc] initWithFrame:textrect];
    text.minSize = NSMakeSize(0.0, textrect.size.height);
    text.maxSize = NSMakeSize(1e9, 1e9);
    text.verticallyResizable = YES;
    text.horizontallyResizable = NO;
    text.textContainer.containerSize = NSMakeSize(textrect.size.width, 1e9);
    text.textContainer.widthTracksTextView = YES;
    text.textContainerInset = NSMakeSize(4,4);
    highlighter = [[DndHighlighter alloc] init];
    text.textStorage.delegate = highlighter;

    NSError* err;
    NSString* str = [NSString stringWithContentsOfFile:testpath encoding:NSUTF8StringEncoding error:&err];
    text.string = str;
    scrollview = [[NSScrollView alloc] initWithFrame:textrect];
    scrollview.borderType = NSNoBorder;
    scrollview.hasVerticalScroller = YES;
    scrollview.hasHorizontalScroller = NO;
    scrollview.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin;
    scrollview.documentView = text;
    [self.view addSubview:scrollview];

    NSRect webrect = {.origin={0, 0}, .size={1400-550, 800}};
    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    // MacOS 11.0
    // config.limitsNavigationsToAppBoundDomains = YES;
    webview = [[WKWebView alloc] initWithFrame:webrect configuration:config];
    [self.view addSubview:webview];
    [self recalc_html:nil];
    // NSURL* url = [[NSURL alloc] initFileURLWithPath:testurl];
    // [webview loadFileURL:url allowingReadAccessToURL:url];
    webview.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

- (void) recalc_html:(id)sender {
    // FIXME: don't do this synchronously
    // FIXME: where the fuck are you supposed to put this stuff.
    NSString *string = self->text.string;
    const char* source_text = [string UTF8String];
    LongString html = {};
    auto err = dndc_make_html((LongString){strlen(source_text), source_text}, &html);
    if(err){
        // NSLog(@"error when doing html: %d", err);
        return;
    }
    NSString* htmlstring = [NSString stringWithUTF8String:html.text];
    const_free(html.text);
    [webview loadHTMLString:htmlstring baseURL:[NSURL URLWithString:@"https://localhost/this.html"]];
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];
}

- (void)loadView {
    // NSRect sframe = NSScreen.mainScreen.frame;
    self.view = [[NSView alloc] initWithFrame: (NSRect){{0, 0}, {1400, 800}}];
}
-(void) save:(id)sender{
    [self recalc_html:sender];
}
-(void) openDocument:(id)sender {
    auto panel = [NSOpenPanel openPanel];
    auto resp = [panel runModal];
    if(resp == NSModalResponseOK){
        NSError* err = NULL;
        NSString* str = [NSString stringWithContentsOfURL: (NSURL*)panel.URL encoding:NSUTF8StringEncoding error:&err];
        text.string = str;
    }
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate> {
    NSWindow* window;
}
@end

static void do_menus(void);
@implementation AppDelegate : NSObject
-(void)applicationWillFinishLaunching:(NSNotification *)notification{
    do_menus();
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    window = [NSWindow.alloc
        initWithContentRect: NSMakeRect(0, 0, 1400, 800)
        styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
        backing: NSBackingStoreBuffered
        defer: NO];
    window.title = @"DndC";
    [window cascadeTopLeftFromPoint: NSMakePoint(20,20)];
    [window makeKeyAndOrderFront: nil];
    window.contentViewController = [[ViewController alloc] init];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}
@end



int main(int argc, const char * argv[]) {
    if(dndc_init_python() != 0)
        return 1;

    chdir("/Users/drpriver/Documents/Dungeons/BarrowMaze/");
    NSApplication* app = [NSApplication sharedApplication];
    AppDelegate* appDelegate = [AppDelegate new]; // cannot collapse this and next line because .dlegate is weak
    app.delegate = appDelegate;
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

    /* Tell the application object that this is now the application menu */
    // it doesn't seem like you need this?
    // [NSApp setAppleMenu:appleMenu];
    // [appleMenu release];

    /* Create the File menu */
    {
        NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
        [fileMenu addItemWithTitle:@"New" action:@selector(newDocument:) keyEquivalent:@"n"];
        [fileMenu addItemWithTitle:@"Open" action:@selector(openDocument:) keyEquivalent:@"o"];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        [fileMenu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
        [fileMenu addItemWithTitle:@"Save" action:@selector(save:) keyEquivalent:@"s"];
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
        [editMenu addItem:[NSMenuItem separatorItem]];
        [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
        // Idk how to do this.
        // [editMenu addItemWithTitle:@"Find" action:@selector(performFindPanelAction:) keyEquivalent:@"f"];
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:editMenu];
        [[NSApp mainMenu] addItem:menuItem];
    }


    /* Create the window menu */
    {
        NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

        /* Add menu items */

        [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];

        [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

        /* Add the fullscreen toggle menu option, if supported */
        if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6) {
            /* Cocoa should update the title to Enter or Exit Full Screen automatically.
             * But if not, then just fallback to Toggle Full Screen.
             */
            NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
            [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagCommand];
            [windowMenu addItem:menuItem];
        }

        /* Put menu into the menubar */
        NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:windowMenu];
        [[NSApp mainMenu] addItem:menuItem];

        /* Tell the application object that this is now the window menu */
        [NSApp setWindowsMenu:windowMenu];
    }
}

