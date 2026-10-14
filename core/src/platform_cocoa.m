/* platform_cocoa.m: macOS through Cocoa (compile with -fobjc-arc). The window's content reaches under a transparent
   title bar, so Rekta's title bar sits beside the system's traffic lights; pixels go up as a CALayer's contents; the
   theme follows the effective appearance and the motion setting the accessibility display options. While a field
   has focus, keys go through the text input system (NSTextInputClient): an input method's composing text is shown
   by the field, underlined, and what it commits is typed; dead keys compose the same way. Notifications go through
   UNUserNotificationCenter, loaded at run time, and only from an app bundle, which it needs. */
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <UserNotifications/UserNotifications.h>
#include "rk_internal.h"

@class RKView;
typedef struct cwin {
  rk_window *w; NSWindow *__unsafe_unretained win; RKView *__unsafe_unretained view;
  bool ime; NSRect caret;                                      /* a field has focus; its caret in the view */
} cwin;

static NSMutableArray *windows;                                /* keeps windows and delegates alive */

static uint32_t mods_of(NSEventModifierFlags f) {
  return ((f & NSEventModifierFlagShift) ? RK_MOD_SHIFT : 0) | ((f & NSEventModifierFlagControl) ? RK_MOD_CTRL : 0) |
         ((f & NSEventModifierFlagOption) ? RK_MOD_ALT : 0) | ((f & NSEventModifierFlagCommand) ? RK_MOD_SUPER : 0);
}

static uint32_t key_of(NSEvent *e) {
  switch (e.keyCode) {
    case 0x30: return RK_KEY_TAB; case 0x24: case 0x4C: return RK_KEY_ENTER; case 0x35: return RK_KEY_ESCAPE; case 0x31: return RK_KEY_SPACE;
    case 0x7B: return RK_KEY_LEFT; case 0x7C: return RK_KEY_RIGHT; case 0x7E: return RK_KEY_UP; case 0x7D: return RK_KEY_DOWN;
    case 0x73: return RK_KEY_HOME; case 0x77: return RK_KEY_END; case 0x74: return RK_KEY_PAGE_UP; case 0x79: return RK_KEY_PAGE_DOWN;
    case 0x33: return RK_KEY_BACKSPACE; case 0x75: return RK_KEY_DELETE;
    case 0x7A: return RK_KEY_F1; case 0x78: return RK_KEY_F1 + 1; case 0x63: return RK_KEY_F1 + 2; case 0x76: return RK_KEY_F1 + 3;
    case 0x60: return RK_KEY_F1 + 4; case 0x61: return RK_KEY_F1 + 5; case 0x62: return RK_KEY_F1 + 6; case 0x64: return RK_KEY_F1 + 7;
    case 0x65: return RK_KEY_F1 + 8; case 0x6D: return RK_KEY_F1 + 9; case 0x67: return RK_KEY_F11; case 0x6F: return RK_KEY_F12;
    case 0x72: return RK_KEY_INSERT;                          /* Help, where Insert is on a PC keyboard */
    default: break;
  }
  NSString *s = e.charactersIgnoringModifiers.uppercaseString;
  if (s.length == 1) { unichar c = [s characterAtIndex:0]; if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c > 0x20 && c < 0x7f)) return c; }
  return RK_KEY_NONE;
}

@interface RKView : NSView <NSTextInputClient>
@property(nonatomic) cwin *owner;
@property(nonatomic, strong) NSString *marked;                 /* what the input method is composing */
@end

static NSString *plain(id s) { return [s isKindOfClass:[NSAttributedString class]] ? ((NSAttributedString *)s).string : (NSString *)s; }

@implementation RKView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)e { (void)e; return YES; }
- (BOOL)wantsUpdateLayer { return YES; }
- (void)updateLayer { if (self.owner) rk__window_invalidate(self.owner->w); }
- (void)updateTrackingAreas {
  for (NSTrackingArea *t in self.trackingAreas) [self removeTrackingArea:t];
  [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:self.bounds
                                                     options:NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited
                                                       owner:self userInfo:nil]];
  [super updateTrackingAreas];
}
- (void)pointer:(NSEvent *)e type:(rk_event_type)type button:(int)b {
  if (!self.owner) return;
  NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
  rk_event ev;
  memset(&ev, 0, sizeof ev);
  ev.size = sizeof ev; ev.type = type; ev.time = rk_app_now(); ev.button = b; ev.mods = mods_of(e.modifierFlags);
  ev.position = (rk_point){ (float)p.x, (float)p.y };
  rk__window_event(self.owner->w, &ev);
}
/* Files dragged in (NSDraggingDestination): each enter and update asks the window whether a view under the pointer
   takes them (a copy) or not; the drop hands it their paths. */
static NSDictionary *files_only(void) { return @{ NSPasteboardURLReadingFileURLsOnlyKey: @YES }; }
- (NSDragOperation)drag:(id<NSDraggingInfo>)info type:(rk_event_type)type {
  if (!self.owner) return NSDragOperationNone;
  NSPoint p = [self convertPoint:info.draggingLocation fromView:nil];
  BOOL files = [info.draggingPasteboard canReadObjectForClasses:@[ [NSURL class] ] options:files_only()];
  rk_event ev;
  memset(&ev, 0, sizeof ev);
  ev.size = sizeof ev; ev.type = type; ev.time = rk_app_now(); ev.position = (rk_point){ (float)p.x, (float)p.y };
  BOOL taken = files && rk__window_drag(self.owner->w, &ev);
  rk__app_request_frame();
  return taken ? NSDragOperationCopy : NSDragOperationNone;
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info { return [self drag:info type:RK_EVENT_DRAG_ENTER]; }
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info { return [self drag:info type:RK_EVENT_DRAG_MOVE]; }
- (void)draggingExited:(id<NSDraggingInfo>)info {
  (void)info;
  if (!self.owner) return;
  rk_event ev;
  memset(&ev, 0, sizeof ev);
  ev.size = sizeof ev; ev.type = RK_EVENT_DRAG_LEAVE; ev.time = rk_app_now();
  rk__window_drag(self.owner->w, &ev);
  rk__app_request_frame();
}
- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info { (void)info; return YES; }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  if (!self.owner) return NO;
  NSArray *urls = [info.draggingPasteboard readObjectsForClasses:@[ [NSURL class] ] options:files_only()];
  int n = (int)urls.count;
  char **paths = n ? rk__alloc((size_t)n * sizeof *paths) : NULL;
  for (int i = 0; i < n; i++) paths[i] = rk__strdup(((NSURL *)urls[(NSUInteger)i]).fileSystemRepresentation);
  NSPoint p = [self convertPoint:info.draggingLocation fromView:nil];
  rk_event ev;
  memset(&ev, 0, sizeof ev);
  ev.size = sizeof ev; ev.type = n ? RK_EVENT_DROP : RK_EVENT_DRAG_LEAVE; ev.time = rk_app_now();
  ev.position = (rk_point){ (float)p.x, (float)p.y };
  ev.files = (const char *const *)paths; ev.nfiles = n;
  BOOL took = rk__window_drag(self.owner->w, &ev) && n;
  rk__free_paths(paths, n);
  rk__app_request_frame();
  return took;
}
- (void)mouseDown:(NSEvent *)e { [self pointer:e type:RK_EVENT_POINTER_DOWN button:0]; }
- (void)mouseUp:(NSEvent *)e { [self pointer:e type:RK_EVENT_POINTER_UP button:0]; }
- (void)mouseDragged:(NSEvent *)e { [self pointer:e type:RK_EVENT_POINTER_MOVE button:0]; }
- (void)mouseMoved:(NSEvent *)e { [self pointer:e type:RK_EVENT_POINTER_MOVE button:0]; }
- (void)rightMouseDown:(NSEvent *)e { [self pointer:e type:RK_EVENT_POINTER_DOWN button:1]; }
- (void)rightMouseUp:(NSEvent *)e { [self pointer:e type:RK_EVENT_POINTER_UP button:1]; }
- (void)otherMouseDown:(NSEvent *)e {
  if (e.buttonNumber == 3 && self.owner) {                   /* the mouse's back button */
    rk_event ev;
    memset(&ev, 0, sizeof ev);
    ev.size = sizeof ev; ev.type = RK_EVENT_KEY_DOWN; ev.time = rk_app_now(); ev.key = RK_KEY_BACK;
    rk__window_event(self.owner->w, &ev);
    return;
  }
  [self pointer:e type:RK_EVENT_POINTER_DOWN button:2];
}
- (void)otherMouseUp:(NSEvent *)e { if (e.buttonNumber != 3) [self pointer:e type:RK_EVENT_POINTER_UP button:2]; }
- (void)scrollWheel:(NSEvent *)e {
  if (!self.owner) return;
  NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
  rk_event ev;
  memset(&ev, 0, sizeof ev);
  ev.size = sizeof ev; ev.type = RK_EVENT_WHEEL; ev.time = rk_app_now(); ev.mods = mods_of(e.modifierFlags);
  ev.position = (rk_point){ (float)p.x, (float)p.y };
  float k = e.hasPreciseScrollingDeltas ? 1.0f : 16.0f;
  ev.wheel_x = (float)-e.scrollingDeltaX * k; ev.wheel_y = (float)-e.scrollingDeltaY * k;
  rk__window_event(self.owner->w, &ev);
}
- (void)keyDown:(NSEvent *)e { [self key:e down:YES]; }
- (void)keyUp:(NSEvent *)e { [self key:e down:NO]; }
- (void)key:(NSEvent *)e down:(BOOL)down {
  if (!self.owner) return;
  rk_event ev;
  memset(&ev, 0, sizeof ev);
  ev.size = sizeof ev; ev.type = down ? RK_EVENT_KEY_DOWN : RK_EVENT_KEY_UP; ev.time = rk_app_now();
  ev.key = key_of(e); ev.mods = mods_of(e.modifierFlags);
  if (down && self.owner->ime && !(ev.mods & (RK_MOD_CTRL | RK_MOD_SUPER))) {
    /* a field has focus: the key is a key for the app (unless something is being composed, then it belongs to the
       input method), and its text comes through the text input system */
    if (ev.key && !self.marked.length) rk__window_event(self.owner->w, &ev);
    if (self.owner && self.owner->ime) [self interpretKeyEvents:@[ e ]];
    return;
  }
  if (ev.key) rk__window_event(self.owner->w, &ev);
  NSString *chars = e.characters;
  if (down && chars.length && !(ev.mods & (RK_MOD_CTRL | RK_MOD_SUPER))) {
    unichar c = [chars characterAtIndex:0];
    if (c >= 0x20 && c != 0x7f && (c < 0xF700 || c > 0xF8FF)) {   /* not a function key's private character */
      rk_event t = ev;
      t.type = RK_EVENT_TEXT; t.key = 0; t.text = chars.UTF8String;
      rk__window_event(self.owner->w, &t);
    }
  }
}

/* NSTextInputClient: the field holds the text; the view only passes on what is composed and what is committed. */
- (void)send:(rk_event_type)type text:(NSString *)s {
  if (!self.owner) return;
  rk_event ev;
  memset(&ev, 0, sizeof ev);
  ev.size = sizeof ev; ev.type = type; ev.time = rk_app_now(); ev.text = s.UTF8String ? s.UTF8String : "";
  rk__window_event(self.owner->w, &ev);
}
- (BOOL)hasMarkedText { return self.marked.length > 0; }
- (NSRange)markedRange { return self.marked.length ? NSMakeRange(0, self.marked.length) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return NSMakeRange(NSNotFound, 0); }
- (void)setMarkedText:(id)s selectedRange:(NSRange)sel replacementRange:(NSRange)rep {
  (void)sel; (void)rep;
  self.marked = [plain(s) copy];
  [self send:RK_EVENT_COMPOSE text:self.marked];
}
- (void)unmarkText {                                           /* what was composed stays: it is typed */
  NSString *t = self.marked;
  self.marked = nil;
  [self send:RK_EVENT_COMPOSE text:@""];
  if (t.length) [self send:RK_EVENT_TEXT text:t];
}
- (void)insertText:(id)s replacementRange:(NSRange)rep {
  (void)rep;
  NSString *t = plain(s);
  if (self.marked.length) { self.marked = nil; [self send:RK_EVENT_COMPOSE text:@""]; }
  if (!t.length) return;
  unichar c = [t characterAtIndex:0];
  if (c < 0x20 || c == 0x7f || (c >= 0xF700 && c <= 0xF8FF)) return;   /* a control or a function key's character */
  [self send:RK_EVENT_TEXT text:t];
}
- (void)doCommandBySelector:(SEL)sel { (void)sel; }            /* Enter, arrows, Backspace: already sent as keys */
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText { return @[]; }
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)r actualRange:(NSRangePointer)a { (void)r; (void)a; return nil; }
- (NSUInteger)characterIndexForPoint:(NSPoint)p { (void)p; return NSNotFound; }
- (NSRect)firstRectForCharacterRange:(NSRange)r actualRange:(NSRangePointer)a {   /* where the candidate window goes */
  (void)r; (void)a;
  if (!self.owner || !self.window) return NSZeroRect;
  return [self.window convertRectToScreen:[self convertRect:self.owner->caret toView:nil]];
}
@end

@interface RKDelegate : NSObject <NSWindowDelegate>
@property(nonatomic) cwin *owner;
@end

@implementation RKDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender { (void)sender; if (self.owner) rk__window_close_request(self.owner->w); return NO; }
- (void)sync {
  if (!self.owner) return;
  NSSize s = self.owner->view.bounds.size;
  rk__window_resized(self.owner->w, (float)s.width, (float)s.height, (float)self.owner->win.backingScaleFactor);
}
- (void)windowDidResize:(NSNotification *)n { (void)n; [self sync]; rk__window_frame(self.owner->w, rk_app_now()); }
- (void)windowDidChangeBackingProperties:(NSNotification *)n { (void)n; [self sync]; }
@end

static void settings_changed(void) { rk__app_settings_changed(); }

static bool c_init(const rk_app_desc *d) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    windows = [NSMutableArray array];
    /* the app menu with Quit (Cmd+Q), as every Mac app has */
    NSMenu *bar = [NSMenu new], *menu = [NSMenu new];
    NSMenuItem *item = [NSMenuItem new];
    [bar addItem:item];
    NSString *name = d && d->name ? [NSString stringWithUTF8String:d->name] : @"Rekta";
    [menu addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(terminate:) keyEquivalent:@"q"];
    item.submenu = menu;
    NSApp.mainMenu = bar;
    [NSApp finishLaunching];
    [[NSDistributedNotificationCenter defaultCenter] addObserverForName:@"AppleInterfaceThemeChangedNotification" object:nil queue:nil
                                                             usingBlock:^(NSNotification *n) { (void)n; settings_changed(); }];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil queue:nil
                                                                usingBlock:^(NSNotification *n) { (void)n; settings_changed(); }];
    [NSApp activateIgnoringOtherApps:YES];
  }
  return true;
}

static void c_shutdown(void) { [windows removeAllObjects]; }

static void *c_open(rk_window *w, const rk_window_desc *d) {
  @autoreleasepool {
    cwin *x = rk__alloc(sizeof *x);
    x->w = w;
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    if (!d->system_frame) style |= NSWindowStyleMaskFullSizeContentView;
    NSWindow *win = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, d->width, d->height) styleMask:style backing:NSBackingStoreBuffered defer:NO];
    win.title = [NSString stringWithUTF8String:d->title ? d->title : ""];
    win.contentMinSize = NSMakeSize(d->min_width, d->min_height);
    win.releasedWhenClosed = NO;
    if (!d->system_frame) { win.titlebarAppearsTransparent = YES; win.titleVisibility = NSWindowTitleHidden; }
    RKView *view = [[RKView alloc] initWithFrame:NSMakeRect(0, 0, d->width, d->height)];
    view.wantsLayer = YES;
    view.layer.contentsGravity = kCAGravityTopLeft;
    view.owner = x;
    [view registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
    win.contentView = view;
    RKDelegate *del = [RKDelegate new];
    del.owner = x;
    win.delegate = del;
    [win center];
    x->win = win; x->view = view;
    [windows addObject:@[ win, del ]];
    [del sync];
    return x;
  }
}

static void c_close(void *n) {
  cwin *x = n;
  if (!x) return;
  x->view.owner = NULL;
  ((RKDelegate *)x->win.delegate).owner = NULL;
  [x->win orderOut:nil];
  [x->win close];
  for (NSArray *pair in [windows copy]) if (pair[0] == x->win) [windows removeObject:pair];
  free(x);
}

static void c_show(void *n) { cwin *x = n; if (x) [x->win makeKeyAndOrderFront:nil]; }
static void c_title(void *n, const char *t) { cwin *x = n; if (x) x->win.title = [NSString stringWithUTF8String:t]; }

static void c_present(void *n, const rk_bitmap *fb, const rk_irect *changed, int nchanged) {
  (void)changed; (void)nchanged;
  cwin *x = n;
  if (!x || !fb) return;
  @autoreleasepool {
    /* 0xAARRGGBB in memory order B, G, R, A: premultiplied first, 32-bit little endian */
    CFDataRef data = CFDataCreate(NULL, (const UInt8 *)fb->px, (CFIndex)fb->stride * fb->h * 4);
    CGDataProviderRef prov = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGImageRef img = CGImageCreate((size_t)fb->w, (size_t)fb->h, 8, 32, (size_t)fb->stride * 4, cs,
                                   kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little, prov, NULL, false, kCGRenderingIntentDefault);
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    x->view.layer.contentsScale = x->win.backingScaleFactor;
    x->view.layer.contents = (__bridge id)img;
    [CATransaction commit];
    CGImageRelease(img); CGColorSpaceRelease(cs); CGDataProviderRelease(prov); CFRelease(data);
  }
}

static void c_move(void *n) { cwin *x = n; NSEvent *e = NSApp.currentEvent; if (x && e) [x->win performWindowDragWithEvent:e]; }
static void c_minimize(void *n) { cwin *x = n; if (x) [x->win miniaturize:nil]; }
static void c_zoom(void *n) { cwin *x = n; if (x) [x->win zoom:nil]; }

static bool c_wait(double ms) {
  @autoreleasepool {
    NSDate *until = ms < 0 ? [NSDate distantFuture] : [NSDate dateWithTimeIntervalSinceNow:ms / 1000.0];
    NSEvent *e = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES];
    while (e) {
      if (e.type != NSEventTypeApplicationDefined) [NSApp sendEvent:e];
      e = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
    }
    [NSApp updateWindows];
  }
  return true;
}

static void c_wake(void) {
  @autoreleasepool {
    NSEvent *e = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0 timestamp:0
                                windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:e atStart:NO];
  }
}

static double c_now(void) { return CACurrentMediaTime() * 1000.0; }

static void c_settings(bool *dark, bool *reduced, bool *compact) {
  *dark = false;
  if (@available(macOS 10.14, *)) {
    NSAppearanceName n = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    *dark = [n isEqualToString:NSAppearanceNameDarkAqua];
  }
  *reduced = [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
  *compact = true;                                             /* a fine pointer: compact density, as the web's media query */
}

static void c_clip_set(const char *utf8) {
  @autoreleasepool {
    NSPasteboard *pb = NSPasteboard.generalPasteboard;
    [pb clearContents];
    [pb setString:[NSString stringWithUTF8String:utf8] forType:NSPasteboardTypeString];
  }
}

static char *c_clip_get(void) {
  @autoreleasepool {
    NSString *s = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
    return s ? rk__strdup(s.UTF8String) : NULL;
  }
}

static void c_cursor(void *n, int shape) {
  (void)n;
  [shape == RK__CURSOR_TEXT ? NSCursor.IBeamCursor : shape == RK__CURSOR_COL_RESIZE ? NSCursor.resizeLeftRightCursor : NSCursor.arrowCursor set];
}

static void c_text_input(void *n, bool on, rk_rect caret) {
  cwin *x = n;
  if (!x) return;
  x->ime = on;
  if (!on) {                                                   /* the field let go: what was being composed goes */
    if (x->view.marked.length) { [x->view.inputContext discardMarkedText]; x->view.marked = nil; }
    return;
  }
  NSRect r = NSMakeRect(caret.x, caret.y, caret.w, caret.h);
  if (NSEqualRects(r, x->caret)) return;
  x->caret = r;
  [x->view.inputContext invalidateCharacterCoordinates];
}

static double c_refresh(void) {
  if (@available(macOS 12.0, *)) { NSInteger n = NSScreen.mainScreen.maximumFramesPerSecond; if (n > 1) return (double)n; }
  return 60;
}

/* The file chooser: an open panel as a sheet on the window; the extensions come from the filter. */
static bool c_choose(void *n, const char *accept, bool multiple, void (*done)(void *, const char *const *, int), void *ctx) {
  cwin *x = n;
  NSOpenPanel *panel = [NSOpenPanel openPanel];
  panel.canChooseFiles = YES; panel.canChooseDirectories = NO; panel.allowsMultipleSelection = multiple;
  char **globs = NULL;
  int ng = rk__accept_globs(accept, &globs);
  if (ng) {
    NSMutableArray *types = [NSMutableArray array];
    for (int i = 0; i < ng; i++) [types addObject:[NSString stringWithUTF8String:globs[i] + 2]];   /* "*.png" -> "png" */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = types;                          /* by extension, as the filter is written */
#pragma clang diagnostic pop
  }
  rk__free_paths(globs, ng);
  void (^answer)(NSModalResponse) = ^(NSModalResponse r) {
    NSArray *urls = r == NSModalResponseOK ? panel.URLs : @[];
    int count = (int)urls.count;
    char **paths = count ? rk__alloc((size_t)count * sizeof *paths) : NULL;
    for (int i = 0; i < count; i++) paths[i] = rk__strdup(((NSURL *)urls[(NSUInteger)i]).fileSystemRepresentation);
    done(ctx, (const char *const *)paths, count);
    rk__free_paths(paths, count);
  };
  if (x && x->win) [panel beginSheetModalForWindow:x->win completionHandler:answer];
  else [panel beginWithCompletionHandler:answer];
  return true;
}

/* Notifications: shown while the app is in front too, as the app asked for them. */
@interface RKNotes : NSObject <UNUserNotificationCenterDelegate>
@end
@implementation RKNotes
- (void)userNotificationCenter:(UNUserNotificationCenter *)center willPresentNotification:(UNNotification *)note
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))show {
  (void)center; (void)note;
  if (@available(macOS 11.0, *)) show(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList);
  else show(UNNotificationPresentationOptionAlert);
}
@end

static bool c_notify(const rk_app_desc *app, const char *title, const char *body) {
  (void)app;
  static RKNotes *notes;
  static Class center_class, content_class, request_class;
  if (!notes) {
    if (!NSBundle.mainBundle.bundleIdentifier) return false;   /* outside a bundle the center throws */
    [[NSBundle bundleWithPath:@"/System/Library/Frameworks/UserNotifications.framework"] load];
    center_class = NSClassFromString(@"UNUserNotificationCenter");
    content_class = NSClassFromString(@"UNMutableNotificationContent");
    request_class = NSClassFromString(@"UNNotificationRequest");
    if (!center_class || !content_class || !request_class) return false;
    notes = [RKNotes new];
    [center_class currentNotificationCenter].delegate = notes;
  }
  UNUserNotificationCenter *center = [center_class currentNotificationCenter];
  NSString *t = [NSString stringWithUTF8String:title] ?: @"", *b = [NSString stringWithUTF8String:body] ?: @"";
  Class content = content_class, request = request_class;
  [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                        completionHandler:^(BOOL granted, NSError *error) {
    (void)error;
    if (!granted) return;                                    /* the person said no once; the system remembers */
    UNMutableNotificationContent *c = [content new];
    c.title = t; c.body = b;
    [center addNotificationRequest:[request requestWithIdentifier:NSUUID.UUID.UUIDString content:c trigger:nil]
             withCompletionHandler:nil];
  }];
  return true;
}

static void c_resize(void *n, float w, float h) { cwin *x = n; if (x && !x->win.zoomed) [x->win setContentSize:NSMakeSize(w, h)]; }

const rk_platform rk__platform_native = {
  "cocoa", c_init, c_shutdown, c_open, c_close, c_show, c_title, c_present, c_move, c_minimize, c_zoom,
  c_wait, c_wake, c_now, c_settings, c_resize, NULL, c_refresh, NULL, c_clip_set, c_clip_get, c_text_input, c_cursor, c_choose, c_notify
};
