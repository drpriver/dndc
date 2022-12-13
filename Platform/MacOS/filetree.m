#import "filetree.h"
#pragma clang assume_nonnull begin
@class FileTree;
@interface FileTreeViewController: NSViewController <NSOutlineViewDelegate, NSOutlineViewDataSource>
-(void)did_double:(FileTree*)sender;
@end

@interface FileTree: NSOutlineView
// This is super hacky and makes things super coupled.
@property BOOL expand_all;
@end


// OutlineView stuff
// -----------------
@interface FileSystemNode : NSObject // <NSToolbarDelegate>

-(id)initWithURL:(NSURL *)url NS_DESIGNATED_INITIALIZER;
@property(readonly) NSURL *URL;
@property(readonly, copy) NSString *displayName;
@property(readonly, strong) NSImage *icon;
@property(readonly, strong) NSArray *children;
@property(readonly) BOOL isDirectory;
@property(readonly) BOOL isPackage;
@property(readonly) BOOL isDnd;
-(void)invalidateChildren;
@end

@interface FileSystemNode ()
@property (strong) NSURL *URL;
@property (assign) BOOL childrenDirty;
@property (strong) NSMutableDictionary *internalChildren;
@end


@implementation FileSystemNode

-(instancetype)init {
    NSAssert(NO, @"Invalid use of init; use initWithURL to create FileSystemNode");
    return [self init];
}

-(id)initWithURL:(NSURL *)url{
    self = [super init];
    if (self != nil) {
        _URL = url;
    }
    return self;
}

-(NSString*)description{
    return [NSString stringWithFormat:@"%@ - %@", super.description, self.URL];
}

-(NSString*)displayName{
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

-(NSImage*)icon{
    NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile:[self.URL path]];
    icon.size = NSMakeSize(16, 16);
    return icon;

}

-(BOOL)isDirectory{
    id value = nil;
    [self.URL getResourceValue:&value forKey:NSURLIsDirectoryKey error:nil];
    return [value boolValue];
}

-(BOOL)isPackage{
    id value = nil;
    [self.URL getResourceValue:&value forKey:NSURLIsPackageKey error:nil];
    return [value boolValue];
}

-(NSArray*)children {
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
                    if(!node.isPackage){
                        BOOL ok = NO;
                        if(!ok) ok = (node.isDirectory && ![filename pathExtension].length);
                        NSString* ext = [filename pathExtension];
                        if(!ok) ok = [ext isEqualToString:@"dnd"];
                        if(!ok) ok = [ext isEqualToString:@"png"];
                        if(!ok) ok = [ext isEqualToString:@"jpg"];
                        if(!ok) ok = [ext isEqualToString:@"jpeg"];
                        if(!ok) ok = [ext isEqualToString:@"css"];
                        if(!ok) ok = [ext isEqualToString:@"js"];
                        if(!ok) ok = [ext isEqualToString:@"txt"];
                        if(!ok) ok = [ext isEqualToString:@"pdf"];

                        if(ok)
                            [newChildren setObject:node forKey:filename];
                    }
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

-(void)invalidateChildren {
    _childrenDirty = YES;
    for (FileSystemNode *child in [self.internalChildren allValues]) {
        [child invalidateChildren];
    }
}
-(BOOL)isDnd{
    return [[self->_URL pathExtension] isEqualToString:@"dnd"];
}

@end

@interface FileTreeCellView : NSTableCellView
@end

@implementation FileTreeCellView

- (id)initWithFrame:(NSRect)frameRect withIdentifier:(NSUserInterfaceItemIdentifier)ident withNode:(FileSystemNode*)node {
    self = [super initWithFrame:frameRect];
    self.autoresizingMask = NSViewNotSizable;
    NSImageView* iv = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 3, 16, 16)];
    NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(21, 3, 400, 14)];
    iv.imageScaling = NSImageScaleProportionallyUpOrDown;
    iv.imageAlignment = NSImageAlignCenter;
    tf.bordered = NO;
    tf.drawsBackground = NO;
    self.imageView = iv;
    self.textField = tf;
    [self addSubview:iv];
    [self addSubview:tf];
    self.identifier = ident;
    self.textField.stringValue = node.displayName;
    self.textField.editable = NO;
    self.imageView.image = node.icon;
    return self;
}
@end

@implementation FileTree
-(BOOL)canBecomeKeyView{
    return YES;
}
-(void)mouseDown:(NSEvent*)event {
        if(event.modifierFlags & NSEventModifierFlagShift)
            self.expand_all = YES;
        else
            self.expand_all = NO;
        [super mouseDown:event];

}
- (void)keyDown:(NSEvent *)event{
    if(event.type == NSEventTypeKeyDown && event.keyCode == 36 && self.doubleAction && self.delegate){
        if(event.modifierFlags & NSEventModifierFlagShift)
            self.expand_all = YES;
        // we're already mega coupled so whatever.
        [(FileTreeViewController*)self.delegate did_double:self];
        return;
    }
    else {
        [super keyDown:event];
    }
}
@end

@implementation FileTreeViewController{
    NSScrollView* scroll_view;
    FileTree* outline;
    FileSystemNode* rootNode;
    NSTableColumn* col;
}
-(void)set_root:(NSURL*)url{
    self->rootNode = [[FileSystemNode alloc] initWithURL:url];
    [self->outline reloadData];
}
-(void)reload{
    self->rootNode = [[FileSystemNode alloc] initWithURL:self->rootNode.URL];
    [self->outline reloadData];
}
-(void)did_double:(FileTree*)sender {
    FileSystemNode* item = [sender itemAtRow: sender.clickedRow];
    if(!item) item = [sender itemAtRow: sender.selectedRow];
    if(!item) return;
    if(item.isDirectory){
        if([sender isItemExpanded:item])
            [sender.animator collapseItem:item collapseChildren:sender.expand_all];
        else{
            [sender.animator expandItem:item expandChildren:sender.expand_all];
        }
        sender.expand_all = NO;
        return;
    }
    if(item.isDnd){
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:item.URL display:YES completionHandler:^(NSDocument *document, BOOL documentWasAlreadyOpen, NSError *error){
            (void)documentWasAlreadyOpen;
            (void)document;
            if(error){
                LOGIT(error);
            }
            else
                [filetree_window makeKeyAndOrderFront:nil];
        }];
        return;
    }
    [[NSWorkspace sharedWorkspace] openURL:item.URL];
}
-(instancetype)initWithURL:(NSURL*)url{
    self = [super init];
    if(!self) return self;
    if(url)
        self->rootNode = [[FileSystemNode alloc] initWithURL:url];
    else
        self->rootNode = nil;
    self->outline = [[FileTree alloc] initWithFrame:NSMakeRect(0, 100, 400, 800)];
    self->outline.allowsColumnResizing = NO;
    self->outline.allowsColumnReordering = NO;
    self->outline.columnAutoresizingStyle = 0;
    self->col = [[NSTableColumn alloc] initWithIdentifier:@"MainCell"];
    self->outline.outlineTableColumn = self->col;
    [self->outline addTableColumn:self->col];
    self->col.editable = NO;
    self->col.minWidth = 400;
    self->outline.indentationPerLevel = 16;
    self->outline.headerView = nil;
    self->outline.indentationMarkerFollowsCell = YES;
    self->outline.doubleAction = @selector(did_double:);

    self->outline.delegate = self;
    self->outline.dataSource = self;
    self->scroll_view = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 400, 800)];
    self->scroll_view.hasVerticalScroller = YES;
    self->scroll_view.hasHorizontalScroller = NO;
    self->scroll_view.autoresizingMask = NSViewHeightSizable|NSViewWidthSizable;
    self->scroll_view.documentView = self->outline;
    self.view = self->scroll_view;
    return self;
}

// NSOutlineViewDataSource
- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(nullable id)item {
    if(!item) item = self->rootNode;
    return [[item children] count];
}
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(nullable id)item{
    if(!item) item = self->rootNode;
    return [[item children] objectAtIndex:index];
}
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item{
    if(!item) item = self->rootNode;
    return [item isDirectory];
}
- (nullable NSView *)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(nullable NSTableColumn *)tableColumn item:(id)item{
    FileTreeCellView* view = [[FileTreeCellView alloc] initWithFrame:NSMakeRect(0, 0, 400, 16) withIdentifier:tableColumn.identifier withNode:item];
    return view;
}
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item{
    return YES;
}
- (nullable id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(nullable NSTableColumn *)tableColumn byItem:(nullable id)item{
    return item; // idk
}

@end

static NSPanel* filetree_window = nil;

static
void
set_filetree_window_url(NSURL* url){
    if(!filetree_window){
        NSRect rect = NSMakeRect(0, 0, 400, 800);
        NSWindow* window = get_main_window();
        if(window){
            rect.origin = window.frame.origin;
            rect.size.height = window.frame.size.height;
        }
        filetree_window = [[NSPanel alloc]
            initWithContentRect:rect
            styleMask: 0
            | NSWindowStyleMaskClosable
            | NSWindowStyleMaskResizable
            | NSWindowStyleMaskTitled
            | NSWindowStyleMaskBorderless
            backing: NSBackingStoreBuffered
            defer: YES];
        if(!filetree_window) return;
        filetree_window.releasedWhenClosed = NO;
        FileTreeViewController* vc = [[FileTreeViewController alloc] initWithURL:url];
        filetree_window.contentViewController = vc;
        filetree_window.floatingPanel = YES;
        [filetree_window standardWindowButton:NSWindowZoomButton].enabled = NO;
    }
    else {
        FileTreeViewController* vc = (id)filetree_window.contentViewController;
        [vc set_root:url];
    }
    [filetree_window makeKeyAndOrderFront:nil];
    [filetree_window center];
}


#pragma clang assume_nonnull end
