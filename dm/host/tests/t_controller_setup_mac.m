// Native flow fixture: real AppKit controls, fake connected devices. Never needs game files or a physical controller.
#import "../controller_setup_apple.h"
#include <assert.h>
// Preview the symbols GameController supplies for a PlayStation pad without claiming hardware testing.
@interface PreviewButton : NSObject
@property(nonatomic, copy) NSString *sfSymbolsName;
@end
@implementation PreviewButton @end
@interface PreviewPad : NSObject
@property(nonatomic) PreviewButton *buttonA, *buttonB, *buttonX, *buttonY;
@end
@implementation PreviewPad @end
@interface PreviewController : NSObject
@property(nonatomic) PreviewPad *extendedGamepad;
@end
@implementation PreviewController @end
@interface PreviewPress : NSObject
@property(nonatomic) NSGestureRecognizerState state;
@property(nonatomic) NSView *view;
- (NSPoint)locationInView:(NSView *)view;
@end
@implementation PreviewPress
- (NSPoint)locationInView:(NSView *)view { return NSMakePoint(NSMidX(view.bounds), NSMidY(view.bounds)); }
@end
static int glyph_updates;
@interface PreviewSetup : DmControllerSetup @end
@implementation PreviewSetup
- (void)glyph:(NSButton *)button symbol:(NSString *)symbol fallback:(NSString *)fallback {
    glyph_updates++; [super glyph:button symbol:symbol fallback:fallback];
}
@end
static uint64_t routes;
static unsigned input;
static int applies;
static int automatic_style = CS_PRO;
uint64_t dm_controller_routes(void) { return routes; }
cs_status dm_controller_preview(uint64_t config, int low, int high, int pair) {
    cs_status st = {.minimum = low, .maximum = high, .allowed = pair ? 0x4 : 0x19, .ids = 15, .sources = 3};
    st.raw_allowed = st.allowed;
    for (int p = 0; p < 8; p++) {
        st.automatic[p] = p < 2 ? cs_encode(automatic_style, p, p) : CS_OFF;
        cs_route r = cs_decode(cs_get(config, p));
        if (r.style > CS_OFF && p < high) st.ready |= 1u << p;
    }
    return st;
}
int dm_controller_apply(uint64_t config, int low, int high, int pair) { if (!cs_can_apply(dm_controller_preview(config, low, high, pair))) return 0; routes = config; applies++; return 1; }
BOOL dm_haptics_apple_identify(GCController *c) { (void)c; return YES; }
static void select_row(DmControllerSetup *ui, int n) { NSArray *rows = [ui valueForKey:@"rows"]; [ui choose:rows[n]]; }
static void check_style(NSButton *tile, NSString *symbol, NSString *second) {
    NSImageView *first = [tile valueForKey:@"symbol"], *other = [tile valueForKey:@"secondSymbol"];
    assert([first.image.TIFFRepresentation isEqual:cs_symbol(symbol, 52).image.TIFFRepresentation]);
    assert((other != nil) == (second != nil));
    if (second) assert([other.image.TIFFRepresentation isEqual:cs_symbol(second, 52).image.TIFFRepresentation]);
}
static void capture(NSView *view, NSString *path) {
    [view display]; NSBitmapImageRep *bitmap = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
    assert(bitmap); [view cacheDisplayInRect:view.bounds toBitmapImageRep:bitmap];
    assert(path); assert([[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path atomically:YES]);
}
int main(int argc, const char **argv) { @autoreleasepool {
    [NSApplication sharedApplication];
    PreviewController *preview = [PreviewController new]; preview.extendedGamepad = [PreviewPad new];
    NSArray *keys = @[@"buttonA", @"buttonB", @"buttonX", @"buttonY"], *symbols = @[@"xmark.circle", @"circle.circle", @"square.circle", @"triangle.circle"];
    for (int i = 0; i < 4; i++) { PreviewButton *b = [PreviewButton new]; b.sfSymbolsName = symbols[i]; [preview.extendedGamepad setValue:b forKey:keys[i]]; }
    cs_navigation_controller = (GCController *)(id)preview;
    assert(cs_symbol(@"missing.controller.symbol", 52).image != nil);
    assert(cs_hold_fill(0) == 0 && cs_hold_fill(0.8) == 1);
    assert(cs_hold_fill(0.2) > 0.5 && cs_hold_fill(0.4) < cs_hold_fill(0.6));
    assert(cs_hold_fill(0.2) - cs_hold_fill(0) > cs_hold_fill(0.6) - cs_hold_fill(0.4));
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1000, 720) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    [window orderFront:nil];
    NSMutableArray *devices = [NSMutableArray new];
    for (int i = 0; i < 2; i++) { DmSetupDevice *d = [DmSetupDevice new]; d.slot = i; d.identity = (const void *)(intptr_t)(i + 1); d.name = i ? @"DUALSHOCK 4 Wireless Controller" : @"DualSense Wireless Controller"; d.motion = YES; d.identify = ^BOOL { return YES; }; [devices addObject:d]; }
    DmControllerSetup *ui = [PreviewSetup new]; ui.devices = ^{ return devices; }; ui.buttons = ^{ return input; }; ui.finished = ^(BOOL ok) { (void)ok; };
    [ui openIn:window.contentView minimum:2 maximum:2 permitPair:1];
    int updates=glyph_updates; [ui refresh]; assert(glyph_updates==updates);
    PreviewPad *original=preview.extendedGamepad; preview.extendedGamepad=[PreviewPad new];
    [ui refresh]; assert(glyph_updates==updates+4); preview.extendedGamepad=original; [ui refresh];
    assert([[ui valueForKey:@"page"] intValue] == 0);
    assert([[[ui valueForKey:@"title"] stringValue] isEqual:@"Set up 2 players"]);
    assert([[ui valueForKey:@"people"] count] == 2);
    for (NSImageView *person in [ui valueForKey:@"people"]) assert(person.alphaValue == 0.25);
    int styles[] = {CS_PRO, CS_LEFT, CS_RIGHT, CS_PAIR, CS_OFF};
    NSArray *icons = @[@"gamecontroller", @"l.joystick", @"r.joystick", @"l.joystick", @"minus.circle"];
    for (int i = 0; i < 5; i++) {
        uint64_t draft = cs_put(0, 0, cs_encode(styles[i], 0, 0));
        [ui setValue:@(draft) forKey:@"draft"]; [ui refresh];
        check_style([ui valueForKey:@"rows"][0], icons[i], styles[i] == CS_PAIR ? @"r.joystick" : nil);
    }
    for (int i = 0; i < 4; i++) {
        automatic_style = styles[i]; [ui setValue:@0 forKey:@"draft"]; [ui refresh];
        check_style([ui valueForKey:@"rows"][0], icons[i], styles[i] == CS_PAIR ? @"r.joystick" : nil);
    }
    automatic_style = CS_PRO; [ui refresh];
    NSButton *applyStart = [ui valueForKey:@"_apply"], *select = [ui valueForKey:@"select"], *backStart = [ui valueForKey:@"_back"];
    assert(NSMaxX(applyStart.frame) < NSMinX(select.frame) && NSMaxX(select.frame) < NSMinX(backStart.frame));
    assert([[applyStart.title stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] isEqual:@"Apply"]);
    [ui advanceHolds:0 active:YES at:1];
    [ui advanceHolds:CSU_BACK active:YES at:2]; [ui advanceHolds:CSU_BACK active:YES at:2.4];
    assert(ui.blocksInput && ![[ui valueForKey:@"settling"] boolValue]);
    [ui advanceHolds:0 active:YES at:2.5];
    assert([[ui valueForKey:@"backFill"] frame].size.width == 0);
    if (argc > 1) {
        capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"players.png"]);
        [ui setValue:@(cs_put(0, 0, cs_encode(CS_PAIR, 0, 0))) forKey:@"draft"]; [ui refresh];
        NSArray<NSImageView *> *lights = [ui valueForKey:@"people"];
        assert(lights[0].alphaValue == 1 && lights[1].alphaValue == 0.25);
        capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"players-configured.png"]);
        [ui setValue:@0 forKey:@"draft"]; [ui refresh];
    }
    select_row(ui, 0); assert([[ui valueForKey:@"page"] intValue] == 1);
    NSButton *selectedTile = [ui valueForKey:@"rows"][0];
    assert([selectedTile.accessibilityLabel containsString:@"◉ Selected"] && ![selectedTile.accessibilityLabel containsString:@"Not ready"]);
    if (argc > 1) capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"types.png"]);
    select_row(ui, 1); assert([[ui valueForKey:@"page"] intValue] == 2);
    if (argc > 1) capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"pair.png"]);
    select_row(ui, 1); assert([[ui valueForKey:@"page"] intValue] == 3);
    NSArray<NSButton *> *automaticRows = [ui valueForKey:@"rows"];
    assert(![automaticRows[0].title containsString:@" • Player "] && ![automaticRows[1].title containsString:@" • Player "]);
    [ui setValue:@(cs_put(0, 1, cs_encode(CS_PRO, 1, 1))) forKey:@"draft"]; [ui refresh];
    NSArray<NSButton *> *assignedRows = [ui valueForKey:@"rows"];
    assert(![assignedRows[0].title containsString:@" • Player "] && [assignedRows[1].title containsString:@" • Player 2"]);
    if (argc > 1) capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"devices.png"]);
    NSArray<NSButton *> *deviceRows = [ui valueForKey:@"rows"];
    [ui focusDevice:deviceRows[1]]; assert([[ui valueForKey:@"page"] intValue] == 3 && !routes);
    select_row(ui, 0); assert([[ui valueForKey:@"page"] intValue] == 4);
    NSArray<NSButton *> *rows = [ui valueForKey:@"rows"]; assert(!rows[0].enabled && rows[1].enabled);
    assert([rows[0].title containsString:@" • Player 1"] && [rows[1].title containsString:@" • Player 2"]);
    select_row(ui, 1); assert([[ui valueForKey:@"page"] intValue] == 0 && !applies && !routes);
    cs_route pair = cs_decode(cs_get([[ui valueForKey:@"draft"] unsignedLongLongValue], 0)); assert(pair.style == CS_PAIR && pair.first == 0 && pair.second == 1);
    [devices removeLastObject]; [ui refresh]; assert(cs_get([[ui valueForKey:@"draft"] unsignedLongLongValue], 0) == CS_OFF);
    [ui cancel]; [ui tick]; assert(!ui.blocksInput && !routes);
    [ui openIn:window.contentView minimum:1 maximum:1 permitPair:0];
    assert([[ui valueForKey:@"page"] intValue] == 1); rows = [ui valueForKey:@"rows"]; assert(!rows[1].enabled && rows[0].enabled);
    [ui cancel]; [ui tick];
    [ui openIn:window.contentView minimum:1 maximum:1 permitPair:1];
    select_row(ui, 1); select_row(ui, 0); select_row(ui, 0);
    assert([[ui valueForKey:@"page"] intValue] == 1);
    NSButton *apply = [ui valueForKey:@"_apply"]; assert(apply.enabled && apply.imageHugsTitle);
    NSRect glyph = [apply.cell imageRectForBounds:apply.bounds]; assert(NSMinX(glyph) >= 18);
    NSTextField *title = [ui valueForKey:@"title"]; NSView *card = [ui valueForKey:@"card"];
    assert(title.alignment == NSTextAlignmentCenter && fabs(NSMidX(title.frame) - NSMidX(card.bounds)) < 0.5);
    double now = NSProcessInfo.processInfo.systemUptime; cs_hold hold = {.since = now - 0.2, .down = 1, .armed = 1};
    [ui setValue:[NSValue valueWithBytes:&hold objCType:@encode(cs_hold)] forKey:@"hold"]; [ui presentHoldAt:now];
    CALayer *fill = [ui valueForKey:@"applyFill"]; assert(fill.frame.size.width > apply.bounds.size.width / 2 && fill.frame.size.width < apply.bounds.size.width);
    if (argc > 1) capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"hold.png"]);
    PreviewPress *press = [PreviewPress new]; press.view = apply; press.state = NSGestureRecognizerStateBegan; [ui held:press];
    assert([[ui valueForKey:@"pointerHeld"] boolValue]);
    press.state = NSGestureRecognizerStateCancelled; [ui held:press];
    assert(![[ui valueForKey:@"pointerHeld"] boolValue] && fill.frame.size.width == 0 && !applies);
    NSButton *back = [ui valueForKey:@"_back"]; [back performClick:nil];
    assert(![[ui valueForKey:@"settling"] boolValue]);
    press.view = back; press.state = NSGestureRecognizerStateBegan; [ui held:press];
    assert([[ui valueForKey:@"pointerBack"] boolValue]);
    press.state = NSGestureRecognizerStateCancelled; [ui held:press];
    assert(![[ui valueForKey:@"pointerBack"] boolValue]);
    NSScrollView *scroll = [ui valueForKey:@"scroll"];
    for (NSScrollerStyle scrollerStyle = NSScrollerStyleLegacy; scrollerStyle <= NSScrollerStyleOverlay; scrollerStyle++) {
        scroll.scrollerStyle = scrollerStyle; [ui layout];
        assert(scroll.documentView.frame.size.width <= scroll.contentSize.width);
        assert(NSMinX(scroll.contentView.bounds) == 0);
    }
    [ui setValue:[NSValue valueWithBytes:&hold objCType:@encode(cs_hold)] forKey:@"backHold"]; [ui presentHoldAt:now];
    if (argc > 1) capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"back-hold.png"]);
    [ui advanceHolds:0 active:YES at:now];
    [window setContentSize:NSMakeSize(780, 390)]; [ui layout];
    assert(NSContainsRect(card.bounds, apply.frame) && NSContainsRect(card.bounds, title.frame));
    if (argc > 1) capture(window.contentView, [@(argv[1]) stringByAppendingPathComponent:@"compact.png"]);
    [ui cancel]; [ui tick];
    [window setContentSize:NSMakeSize(1000, 720)];
    [ui openIn:window.contentView minimum:2 maximum:2 permitPair:1]; select_row(ui, 0);
    [ui advanceHolds:0 active:YES at:10];
    [ui advanceHolds:CSU_BACK active:YES at:11];
    assert([[ui valueForKey:@"page"] intValue] == 0); // Back is instant
    [ui advanceHolds:CSU_BACK active:YES at:11.79];
    [ui advanceHolds:CSU_BACK active:YES at:11.81]; assert([[ui valueForKey:@"page"] intValue] == 0);
    [ui advanceHolds:CSU_BACK active:YES at:14]; assert(![[ui valueForKey:@"settling"] boolValue]);
    [ui advanceHolds:0 active:YES at:15];
    [ui advanceHolds:CSU_BACK active:NO at:16]; [ui advanceHolds:CSU_BACK active:YES at:18];
    assert(![[ui valueForKey:@"settling"] boolValue]);
    [ui advanceHolds:0 active:YES at:19]; [ui advanceHolds:CSU_BACK active:YES at:20];
    input = CSU_BACK; [ui advanceHolds:CSU_BACK active:YES at:20.81]; [ui tick]; assert(ui.blocksInput);
    input = 0; [ui tick]; assert(!ui.blocksInput && !applies);
    [ui openIn:window.contentView minimum:1 maximum:1 permitPair:1];
    select_row(ui, 1); select_row(ui, 0); select_row(ui, 0);
    [ui advanceHolds:0 active:YES at:30]; [ui advanceHolds:CSU_BACK | CSU_APPLY active:YES at:31];
    [ui advanceHolds:CSU_BACK | CSU_APPLY active:YES at:31.81];
    assert([[ui valueForKey:@"settling"] boolValue] && !applies); [ui tick];
    [ui openIn:window.contentView minimum:1 maximum:8 permitPair:1];
    assert([[ui valueForKey:@"people"] count] == 8);
    [window setContentSize:NSMakeSize(390, 720)]; [ui layout];
    NSArray<NSView *> *people = [ui valueForKey:@"people"]; NSView *panel = [ui valueForKey:@"card"]; NSTextField *heading = [ui valueForKey:@"title"];
    for (NSView *person in people) assert(NSContainsRect(panel.bounds, person.frame) && NSMaxY(person.frame) <= NSMinY(heading.frame));
    [ui cancel]; [ui tick];
    puts("native controller setup: pages, pair selection, disabled types, unplug, cancel, single-player skip passed");
} }
