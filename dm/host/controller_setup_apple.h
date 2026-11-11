// Native views for the existing controller applet. Shared flow on Mac and iPhone; platform controls keep pointer, touch and accessibility support.
#pragma once
#import <TargetConditionals.h>
#import <GameController/GameController.h>
#import <QuartzCore/QuartzCore.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#define CSView UIView
#define CSButton UIButton
#define CSLabel UILabel
#define CSColor UIColor
#define CSScroll UIScrollView
#define CSImageView UIImageView
#else
#import <Cocoa/Cocoa.h>
#define CSView NSView
#define CSButton NSButton
#define CSLabel NSTextField
#define CSColor NSColor
#define CSScroll NSScrollView
#define CSImageView NSImageView
#endif
#include "controller_setup.h"
#include <math.h>
#import "haptics_apple.h"

enum { CSU_SELECT = 1, CSU_BACK = 2, CSU_APPLY = 4, CSU_IDENTIFY = 8, CSU_UP = 16, CSU_DOWN = 32, CSU_LEFT = 64, CSU_RIGHT = 128 };
@interface DmSetupDevice : NSObject
@property(nonatomic) int slot;
@property(nonatomic) const void *identity;
@property(nonatomic, copy) NSString *name;
@property(nonatomic) BOOL motion;
@property(nonatomic) GCController *controller;
@property(nonatomic, copy) BOOL (^identify)(void);
@end
@implementation DmSetupDevice @end

static CSColor *cs_color(CGFloat white) { return [CSColor colorWithWhite:white alpha:1]; }
static NSString *cs_style_name(int style) {
    switch (style) { case CS_PRO: return @"Pro Controller"; case CS_PAIR: return @"Joy-Con Pair"; case CS_LEFT: return @"Joy-Con (L)"; case CS_RIGHT: return @"Joy-Con (R)"; default: return @"No controller"; }
}
static NSArray<NSString *> *cs_style_symbols(int style) {
    switch (style) {
    case CS_PRO: return @[@"gamecontroller"];
    case CS_PAIR: return @[@"l.joystick", @"r.joystick"];
    case CS_LEFT: return @[@"l.joystick"];
    case CS_RIGHT: return @[@"r.joystick"];
    default: return @[@"minus.circle"];
    }
}
static int cs_manual_player(uint64_t routes, int source) {
    if (source < 0 || source >= DM_PLAYERS) return -1;
    for (int p = 0; p < DM_PLAYERS; p++) if (cs_get(routes, p) > CS_OFF && (cs_sources(cs_decode(cs_get(routes, p))) >> source & 1)) return p;
    return -1;
}
static CSLabel *cs_label(NSString *text, CGFloat size, BOOL bold) {
#if TARGET_OS_IPHONE
    UILabel *v = [UILabel new]; v.text = text; v.numberOfLines = 0; v.textColor = UIColor.whiteColor; v.textAlignment = NSTextAlignmentCenter;
    v.font = [UIFont systemFontOfSize:size weight:bold ? UIFontWeightSemibold : UIFontWeightRegular];
#else
    NSTextField *v = [NSTextField wrappingLabelWithString:text]; v.textColor = NSColor.whiteColor; v.alignment = NSTextAlignmentCenter;
    v.font = [NSFont systemFontOfSize:size weight:bold ? NSFontWeightSemibold : NSFontWeightRegular];
#endif
    return v;
}
static void cs_text(CSLabel *view, NSString *text) {
#if TARGET_OS_IPHONE
    view.text = text;
#else
    view.stringValue = text;
#endif
}
// The fill is feedback for the same 0.8-second hold, not a shorter confirmation timer.
static CGFloat cs_hold_fill(double seconds) {
    double t = fmax(0, fmin(1, seconds / 0.8));
    return (CGFloat)(1 - (1 - t) * (1 - t) * (1 - t));
}
static CSImageView *cs_symbol(NSString *name, CGFloat size) {
#if TARGET_OS_IPHONE
    UIImage *image = [UIImage systemImageNamed:name withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:size weight:UIImageSymbolWeightRegular]];
    CSImageView *v = [[UIImageView alloc] initWithImage:image ?: [UIImage systemImageNamed:@"gamecontroller"]];
    v.contentMode = UIViewContentModeScaleAspectFit; v.tintColor = UIColor.whiteColor;
#else
    NSImage *image = [NSImage imageWithSystemSymbolName:name accessibilityDescription:nil] ?: [NSImage imageWithSystemSymbolName:@"gamecontroller" accessibilityDescription:nil];
    image = [image imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:size weight:NSFontWeightRegular]];
    CSImageView *v = [NSImageView new]; v.image = image; v.imageScaling = NSImageScaleProportionallyUpOrDown; v.contentTintColor = NSColor.whiteColor;
#endif
    return v;
}
// Separate native labels put the symbol between the player's name and its controller style.
@interface DmSetupTile : CSButton {
    CSLabel *_heading, *_detail, *_status;
    CSImageView *_symbol, *_secondSymbol;
}
- (void)heading:(NSString *)heading detail:(NSString *)detail status:(NSString *)status symbol:(NSString *)symbol second:(NSString *)second;
- (void)arrange;
@end
@implementation DmSetupTile
- (void)heading:(NSString *)heading detail:(NSString *)detail status:(NSString *)status symbol:(NSString *)symbol second:(NSString *)second {
#if TARGET_OS_IPHONE
    UIButtonConfiguration *cfg = self.configuration; cfg.title = nil; self.configuration = cfg;
#else
    self.title = @"";
#endif
    self.accessibilityLabel = [@[heading, detail, status] componentsJoinedByString:@", "];
    _heading = cs_label(heading, 21, YES); _detail = cs_label(detail, 16, NO); _status = cs_label(status, 14, YES);
    for (CSLabel *label in @[_heading, _detail, _status]) [self addSubview:label];
    _symbol = cs_symbol(symbol, 52); [self addSubview:_symbol];
    if (second) { _secondSymbol = cs_symbol(second, 52); [self addSubview:_secondSymbol]; }
#if TARGET_OS_IPHONE
    for (UIView *view in self.subviews) view.userInteractionEnabled = NO;
#else
    self.accessibilityElement = YES;
#endif
}
#if !TARGET_OS_IPHONE
- (BOOL)isFlipped { return YES; }
- (NSView *)hitTest:(NSPoint)p { return NSPointInRect(p, self.frame) ? self : nil; }
#endif
- (void)arrange {
    CGFloat w = self.bounds.size.width, h = self.bounds.size.height;
    _heading.frame = CGRectMake(12, 15, w - 24, 28);
    CGFloat iconH = h >= 190 ? 64 : 44, iconY = h >= 190 ? 56 : 49;
    CGFloat iconW = _secondSymbol ? iconH : iconH * 1.4;
    _symbol.frame = CGRectMake(w / 2 - (_secondSymbol ? iconW + 5 : iconW / 2), iconY, iconW, iconH);
    if (_secondSymbol) _secondSymbol.frame = CGRectMake(w / 2 + 5, iconY, iconW, iconH);
    CGFloat detailY = iconY + iconH + 12;
    _detail.frame = CGRectMake(12, detailY, w - 24, 40);
    _status.frame = CGRectMake(12, h - 30, w - 24, 20);
}
@end
static __weak GCController *cs_navigation_controller;
static unsigned cs_gc_buttons(void) {
    unsigned buttons = 0;
    for (GCController *c in GCController.controllers) {
        GCExtendedGamepad *g = c.extendedGamepad; if (!g) continue;
        if (g.buttonA.isPressed || g.buttonB.isPressed || g.buttonX.isPressed || g.buttonY.isPressed || g.dpad.xAxis.value || g.dpad.yAxis.value || fabsf(g.leftThumbstick.xAxis.value) > 0.6 || fabsf(g.leftThumbstick.yAxis.value) > 0.6) cs_navigation_controller = c;
        if (g.buttonA.isPressed) buttons |= CSU_SELECT;
        if (g.buttonB.isPressed) buttons |= CSU_BACK;
        if (g.buttonY.isPressed) buttons |= CSU_APPLY;
        if (g.buttonX.isPressed) buttons |= CSU_IDENTIFY;
        if (g.dpad.up.isPressed || g.leftThumbstick.yAxis.value > 0.6) buttons |= CSU_UP;
        if (g.dpad.down.isPressed || g.leftThumbstick.yAxis.value < -0.6) buttons |= CSU_DOWN;
        if (g.dpad.left.isPressed || g.leftThumbstick.xAxis.value < -0.6) buttons |= CSU_LEFT;
        if (g.dpad.right.isPressed || g.leftThumbstick.xAxis.value > 0.6) buttons |= CSU_RIGHT;
    }
    return buttons;
}
static DmSetupDevice *cs_gc_device(GCController *c, int slot) {
    DmSetupDevice *d = [DmSetupDevice new]; d.slot = slot; d.identity = (__bridge const void *)c;
    d.name = c.vendorName ?: @"Controller"; d.motion = c.motion.hasRotationRate; d.controller = c;
    if (c.haptics || c.light) d.identify = ^BOOL { return dm_haptics_apple_identify(c); };
    return d;
}
@interface DmSetupSurface : CSView @end
@implementation DmSetupSurface
#if !TARGET_OS_IPHONE
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (void)keyDown:(NSEvent *)e { (void)e; } // front_mac's key monitor owns keyboard navigation
#endif
@end

@interface DmControllerSetup : NSObject {
    DmSetupSurface *_view, *_card, *_content;
    CSScroll *_scroll;
    CSLabel *_title;
    NSMutableArray<CSImageView *> *_people;
    CSButton *_back, *_select, *_apply, *_identify;
    NSMutableArray<CSButton *> *_rows;
    NSArray<DmSetupDevice *> *_available;
    NSArray<NSNumber *> *_values;
    NSTimer *_timer;
    uint64_t _draft;
    cs_status _status;
    cs_hold _hold, _backHold;
    int _minimum, _maximum, _permitPair, _page, _player, _focus, _style, _first, _two, _previous, _keys;
    double _repeatAt;
    BOOL _settling, _ok, _pointerHeld, _pointerBack;
    CALayer *_applyFill, *_backFill;
    NSString *_signature;
    const void *_sourceIdentity[DM_PLAYERS];
}
@property(nonatomic, copy) NSArray<DmSetupDevice *> *(^devices)(void);
@property(nonatomic, copy) unsigned (^buttons)(void);
@property(nonatomic, copy) void (^finished)(BOOL ok);
- (void)openIn:(CSView *)parent minimum:(int)minimum maximum:(int)maximum permitPair:(int)pair;
- (BOOL)blocksInput;
- (void)key:(unsigned)button down:(BOOL)down;
- (void)cancel;
- (void)requirementsMinimum:(int)low maximum:(int)high permitPair:(int)pair;
@end

@implementation DmControllerSetup
- (CSButton *)button:(NSString *)title action:(SEL)action {
    return [self button:title action:action tile:NO];
}
- (CSButton *)button:(NSString *)title action:(SEL)action tile:(BOOL)tile {
#if TARGET_OS_IPHONE
    UIButtonConfiguration *cfg = UIButtonConfiguration.grayButtonConfiguration; cfg.title = title; cfg.cornerStyle = UIButtonConfigurationCornerStyleLarge;
    cfg.baseForegroundColor = UIColor.whiteColor;
    UIButton *b = [tile ? DmSetupTile.class : UIButton.class buttonWithConfiguration:cfg primaryAction:nil]; b.titleLabel.numberOfLines = 0;
    [b addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
#else
    NSButton *b = [tile ? DmSetupTile.class : NSButton.class buttonWithTitle:title target:self action:action]; b.imageHugsTitle = YES; b.bordered = NO; b.wantsLayer = YES;
    b.font = [NSFont systemFontOfSize:17 weight:NSFontWeightMedium]; b.contentTintColor = NSColor.whiteColor;
    b.cell.wraps = YES; b.cell.truncatesLastVisibleLine = YES;
#endif
    b.layer.cornerRadius = 16; return b;
}
- (void)openIn:(CSView *)parent minimum:(int)minimum maximum:(int)maximum permitPair:(int)pair {
    if (_view) return;
    _settling = NO; _ok = NO; _pointerHeld = _pointerBack = NO; _keys = 0; _signature = nil;
    _minimum = minimum; _maximum = maximum; _permitPair = pair;
    _draft = dm_controller_routes(); _page = maximum == 1 ? 1 : 0; _first = -1; _player = 0;
    _previous = (int)self.buttons(); _hold = _backHold = (cs_hold){0};
    _view = [[DmSetupSurface alloc] initWithFrame:parent.bounds];
#if TARGET_OS_IPHONE
    _view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight; _view.backgroundColor = [UIColor colorWithWhite:0 alpha:0.65];
#else
    _view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable; _view.wantsLayer = YES; _view.layer.backgroundColor = [NSColor colorWithWhite:0 alpha:0.65].CGColor;
#endif
    _card = [DmSetupSurface new];
#if TARGET_OS_IPHONE
    _card.backgroundColor = cs_color(0.10);
#else
    _card.wantsLayer = YES; _card.layer.backgroundColor = cs_color(0.10).CGColor;
#endif
    _card.layer.cornerRadius = 28; [_view addSubview:_card];
    _title = cs_label(@"Controllers", 27, YES); [_card addSubview:_title];
    _people = [NSMutableArray new];
    _content = [DmSetupSurface new]; _scroll = [CSScroll new];
#if TARGET_OS_IPHONE
    [_scroll addSubview:_content];
#else
    _scroll.documentView = _content; _scroll.hasVerticalScroller = YES; _scroll.drawsBackground = NO;
#endif
    [_card addSubview:_scroll]; _rows = [NSMutableArray new];
    _back = [self button:@"Cancel" action:@selector(noop:)]; _apply = [self button:@"Apply" action:@selector(noop:)];
    _select = [self button:@"Select" action:@selector(select:)];
    _identify = [self button:@"Identify" action:@selector(identify:)];
    [_card addSubview:_apply]; [_card addSubview:_select]; [_card addSubview:_back]; [_card addSubview:_identify];
    for (CSButton *button in @[_apply, _back]) {
#if TARGET_OS_IPHONE
        UILongPressGestureRecognizer *hold = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(held:)];
        button.accessibilityHint = @"Hold for 0.8 seconds";
#else
        NSPressGestureRecognizer *hold = [[NSPressGestureRecognizer alloc] initWithTarget:self action:@selector(held:)];
        button.accessibilityHelp = @"Hold for 0.8 seconds";
#endif
        button.layer.masksToBounds = YES;
        CALayer *fill = [CALayer layer]; fill.backgroundColor = [CSColor colorWithWhite:1 alpha:0.22].CGColor;
        [button.layer addSublayer:fill];
        if (button == _apply) _applyFill = fill; else _backFill = fill;
        hold.minimumPressDuration = 0; [button addGestureRecognizer:hold];
    }
    [parent addSubview:_view];
    [self refresh];
    __weak DmControllerSetup *weak = self;
    _timer = [NSTimer timerWithTimeInterval:1.0 / 60 repeats:YES block:^(NSTimer *t) { (void)t; [weak tick]; }];
    [NSRunLoop.mainRunLoop addTimer:_timer forMode:NSRunLoopCommonModes];
}
- (void)requirementsMinimum:(int)low maximum:(int)high permitPair:(int)pair {
    _minimum = low; _maximum = high; _permitPair = pair; _page = high == 1 ? 1 : 0; _player = _focus = 0; _signature = nil; [self refresh];
}
- (BOOL)blocksInput { return _view != nil; }
- (void)key:(unsigned)button down:(BOOL)down { _keys = down ? _keys | (int)button : _keys & ~(int)button; [self tick]; }
- (void)select:(id)sender { (void)sender; if (_focus < (int)_rows.count) [self choose:_rows[_focus]]; }
- (void)noop:(id)sender { (void)sender; }
- (void)held:(id)gesture {
#if TARGET_OS_IPHONE
    UIGestureRecognizerState state = ((UILongPressGestureRecognizer *)gesture).state;
    BOOL holding = state == UIGestureRecognizerStateBegan || state == UIGestureRecognizerStateChanged;
#else
    NSGestureRecognizerState state = ((NSPressGestureRecognizer *)gesture).state;
    BOOL holding = state == NSGestureRecognizerStateBegan || state == NSGestureRecognizerStateChanged;
#endif
    CSButton *button = (CSButton *)[gesture view];
    BOOL down = holding && CGRectContainsPoint(button.bounds, [gesture locationInView:button]);
    if (button == _back) _pointerBack = down; else _pointerHeld = down;
    [self tick];
}
- (void)presentHoldAt:(double)now {
    [CATransaction begin]; [CATransaction setDisableActions:YES];
    _applyFill.frame = CGRectMake(0, 0, _apply.bounds.size.width * (_hold.down ? cs_hold_fill(now - _hold.since) : 0), _apply.bounds.size.height);
    _backFill.frame = CGRectMake(0, 0, _back.bounds.size.width * (_backHold.down ? cs_hold_fill(now - _backHold.since) : 0), _back.bounds.size.height);
    [CATransaction commit];
}
- (void)advanceHolds:(unsigned)buttons active:(BOOL)active at:(double)now {
    BOOL backing = (buttons & CSU_BACK) != 0;
    BOOL cancel = _page == 0 || (_page == 1 && _maximum == 1);
    if (!cancel && backing && _backHold.armed && active) [self back:nil];
    else if (cs_hold_step(&_backHold, backing, active && cancel, now)) [self back:nil];
    else if (cs_hold_step(&_hold, buttons & CSU_APPLY, active && _apply.enabled && !backing, now)) [self apply];
    [self presentHoldAt:now];
}
- (void)finish:(BOOL)ok {
    if (_settling) return;
    _settling = YES; _ok = ok; _card.hidden = YES;
    // The scrim keeps input until every closing button is released, as ctrl_prompt's settle phase does.
}
- (void)cancel { [self finish:NO]; }
- (void)apply {
    if (_apply.enabled && dm_controller_apply(_draft, _minimum, _maximum, _permitPair)) [self finish:YES];
    else { _hold = _backHold = (cs_hold){0}; [self refresh]; }
}
- (void)back:(id)sender {
    (void)sender; _hold = _backHold = (cs_hold){0};
    if (_page == 0 || (_page == 1 && _maximum == 1)) { [self cancel]; return; }
    _page = _page == 1 ? 0 : _page == 2 ? 1 : _page == 4 ? 3 : _style == CS_PAIR ? 2 : 1;
    _focus = 0; _signature = nil; [self refresh];
}
- (void)identify:(id)sender {
    (void)sender;
    if (_page < 3 || _focus >= (int)_values.count) return;
    DmSetupDevice *device = _available[_values[_focus].intValue];
    if (device.identify && device.identify()) {
#if TARGET_OS_IPHONE
        UIButtonConfiguration *cfg = _identify.configuration; cfg.title = @"Signal sent"; _identify.configuration = cfg;
#else
        _identify.title = @"  Signal sent";
#endif
        _identify.accessibilityLabel = @"Identify, signal sent";
    }
}
- (void)focusDevice:(CSButton *)sender { _focus = (int)sender.tag; _hold = _backHold = (cs_hold){0}; [self focus]; }
- (void)choose:(CSButton *)sender {
    _focus = (int)sender.tag; if (!sender.enabled || _focus < 0 || _focus >= (int)_values.count) return;
    int value = _values[_focus].intValue; _hold = _backHold = (cs_hold){0};
    if (_page == 0) { _player = value; _page = 1; }
    else if (_page == 1) { _style = value; _two = 0; _page = value == CS_PAIR ? 2 : 3; }
    else if (_page == 2) { _two = value; _page = 3; }
    else {
        DmSetupDevice *device = _available[value];
        if (_page == 3 && _two) { _first = device.slot; _page = 4; }
        else {
            int first = _page == 4 ? _first : device.slot;
            uint32_t chosen = (1u << first) | (1u << device.slot);
            for (int p = 0; p < DM_PLAYERS; p++) if (p != _player && cs_get(_draft, p) > CS_OFF && (cs_sources(cs_decode(cs_get(_draft, p))) & chosen)) _draft = cs_put(_draft, p, CS_OFF);
            _draft = cs_put(_draft, _player, cs_encode(_style, first, device.slot));
            _page = _maximum == 1 ? 1 : 0;
        }
    }
    _focus = 0; _signature = nil; [self refresh];
}
- (void)move:(int)delta {
    int n = (int)_rows.count; if (!n) return;
    int next = _focus;
    for (int k = 0; k < n; k++) { next = (next + delta + n) % n; if (_page != 1 || _rows[next].enabled) break; }
    _focus = next; _hold = _backHold = (cs_hold){0}; [self focus];
}
- (void)focus {
    for (int i = 0; i < (int)_rows.count; i++) {
        CSButton *b = _rows[i]; BOOL selected = i == _focus;
        b.layer.borderWidth = selected ? 3 : 0; b.layer.borderColor = cs_color(0.95).CGColor;
#if TARGET_OS_IPHONE
        b.alpha = b.enabled ? 1 : 0.35;
#else
        b.alphaValue = b.enabled ? 1 : 0.35; b.layer.backgroundColor = cs_color(selected ? 0.25 : 0.16).CGColor;
#endif
    }
    #if TARGET_OS_IPHONE
    UIButtonConfiguration *cfg = _identify.configuration; cfg.title = @"Identify"; _identify.configuration = cfg;
#else
    _identify.title = @"  Identify";
#endif
    BOOL devicePage = _page >= 3 && _focus < (int)_values.count;
    _identify.hidden = !devicePage;
    _identify.enabled = devicePage && _available[_values[_focus].intValue].identify != nil;
    if (_focus < (int)_rows.count) {
#if TARGET_OS_IPHONE
        [_scroll scrollRectToVisible:_rows[_focus].frame animated:NO];
        UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, _rows[_focus]);
#else
        [_content scrollRectToVisible:_rows[_focus].frame];
        NSAccessibilityPostNotification(_rows[_focus], NSAccessibilityFocusedUIElementChangedNotification);
#endif
    }
}
- (void)glyph:(CSButton *)button symbol:(NSString *)symbol fallback:(NSString *)fallback {
#if TARGET_OS_IPHONE
    UIButtonConfiguration *cfg = button.configuration; cfg.image = [UIImage systemImageNamed:symbol ?: fallback]; cfg.imagePadding = 9; cfg.contentInsets = NSDirectionalEdgeInsetsMake(10, 18, 10, 18); button.configuration = cfg;
#else
    button.image = [NSImage imageWithSystemSymbolName:symbol ?: fallback accessibilityDescription:nil]; button.imagePosition = NSImageLeading; button.imageHugsTitle = YES;
    // AppKit has no imagePadding; keep a small gap inside the centered group.
    if (![button.title hasPrefix:@"  "]) button.title = [@"  " stringByAppendingString:button.title];
#endif
}
- (void)refresh {
    if (_settling) return;
    _available = self.devices();
    const void *identities[DM_PLAYERS] = {0};
    for (DmSetupDevice *d in _available) if (d.slot >= 0 && d.slot < DM_PLAYERS) identities[d.slot] = d.identity;
    for (int source = 0; source < DM_PLAYERS; source++) {
        if (_sourceIdentity[source] && identities[source] != _sourceIdentity[source])
            for (int p = 0; p < DM_PLAYERS; p++) if (cs_sources(cs_decode(cs_get(_draft, p))) >> source & 1) _draft = cs_put(_draft, p, CS_OFF);
        _sourceIdentity[source] = identities[source];
    }
    _status = dm_controller_preview(_draft, _minimum, _maximum, _permitPair);
    NSMutableArray<NSString *> *texts = [NSMutableArray new]; NSMutableArray<NSNumber *> *values = [NSMutableArray new], *enabled = [NSMutableArray new];
    NSString *title;
    if (_page == 0) {
        NSString *count = _status.minimum == _status.maximum ? [NSString stringWithFormat:@"%d", _status.maximum] : [NSString stringWithFormat:@"%d–%d", _status.minimum, _status.maximum];
        title = [NSString stringWithFormat:@"Set up %@ players", count];
        for (int p = 0; p < _status.maximum; p++) if (_status.ids >> p & 1 || (p == 0 && _status.ids & 256)) {
            unsigned code = cs_get(_draft, p); cs_route r = cs_decode(code ? code : _status.automatic[p]);
            [texts addObject:[NSString stringWithFormat:@"Player %d\n%@%@\n%@", p + 1, cs_style_name(r.style), code ? @"" : @" • Automatic", _status.ready >> p & 1 ? @"✓ Ready" : @"× Not ready"]];
            [values addObject:@(p)]; [enabled addObject:@YES];
        }
    } else if (_page == 1) {
        title = [NSString stringWithFormat:@"Player %d · Controller type", _player + 1];
        int styles[] = {CS_PRO, CS_PAIR, CS_LEFT, CS_RIGHT};
        unsigned code = cs_get(_draft, _player); cs_route selected = cs_decode(code ? code : _status.automatic[_player]);
        for (int i = 0; i < 4; i++) {
            NSString *tag = selected.style == styles[i] ? @"\n◉ Selected" : @"";
            [texts addObject:[cs_style_name(styles[i]) stringByAppendingString:tag]]; [values addObject:@(styles[i])]; [enabled addObject:@((_status.allowed & cs_style(styles[i])) != 0)];
        }
    } else if (_page == 2) {
        title = @"Joy-Con Pair";
        [texts addObjectsFromArray:@[@"One controller\nGamepad or an existing Joy-Con pair", @"Two controllers\nOne device for each hand"]];
        [values addObjectsFromArray:@[@0, @1]]; [enabled addObjectsFromArray:@[@YES, @YES]];
    } else {
        title = _two ? (_page == 4 ? @"Choose the right controller" : @"Choose the left controller") : @"Choose a controller";
        for (int i = 0; i < (int)_available.count; i++) {
            DmSetupDevice *d = _available[i];
            int assigned = _page == 4 && d.slot >= 0 && d.slot == _first ? _player : cs_manual_player(_draft, d.slot);
            NSString *assignment = assigned >= 0 ? [NSString stringWithFormat:@" • Player %d", assigned + 1] : @"";
            uint32_t need = _style == CS_PRO ? 0u : _style == CS_LEFT ? 1u : _style == CS_RIGHT ? 2u : _two ? (_page == 4 ? 2u : 1u) : 3u;
            BOOL needTwo = _style != CS_PRO && !(_status.raw_allowed & cs_style(_style));
            uint32_t native = d.slot >= 0 ? _status.native_sides[d.slot] : 0;
            BOOL capable = (!needTwo || (native & DM_JOY_2)) && (!(native & 3) || (native & need) == need);
            NSString *suffix = !capable ? (needTwo ? @" • Joy-Con 2 required" : @" • Wrong Joy-Con side") : _page == 4 && d.slot == _first ? @" • Left controller" : d.slot == -2 ? @" • Controller merging is enabled" : d.slot < 0 ? @" • Press a button to join" : @"";
            [texts addObject:[NSString stringWithFormat:@"%@%@%@%@", d.name, assignment, d.motion ? @" • Motion supported" : @"", suffix]];
            [values addObject:@(i)]; [enabled addObject:@(d.slot >= 0 && capable && !(_page == 4 && d.slot == _first))];
        }
        if (!texts.count) title = @"No controllers connected";
    }
    cs_text(_title, title);
    if (_people.count != (NSUInteger)_status.maximum) {
        for (CSImageView *person in _people) [person removeFromSuperview];
        [_people removeAllObjects];
        for (int p = 0; p < _status.maximum; p++) { CSImageView *person = cs_symbol(@"person.fill", 20); [_card addSubview:person]; [_people addObject:person]; }
    }
    GCExtendedGamepad *pad = cs_navigation_controller.extendedGamepad;
    if (!pad) for (DmSetupDevice *d in _available) if (d.controller.extendedGamepad) { pad = d.controller.extendedGamepad; break; }
#if TARGET_OS_IPHONE
    UIButtonConfiguration *back = _back.configuration; back.title = _page == 0 || (_page == 1 && _maximum == 1) ? @"Cancel" : @"Back"; _back.configuration = back;
#else
    _back.title = _page == 0 || (_page == 1 && _maximum == 1) ? @"  Cancel" : @"  Back";
#endif
#if TARGET_OS_IPHONE
    _back.accessibilityHint = _page == 0 || (_page == 1 && _maximum == 1) ? @"Hold for 0.8 seconds" : nil;
#else
    _back.accessibilityHelp = _page == 0 || (_page == 1 && _maximum == 1) ? @"Hold for 0.8 seconds" : nil;
#endif
    _apply.enabled = (_page == 0 || (_page == 1 && _maximum == 1)) && cs_can_apply(_status);
#if TARGET_OS_IPHONE
    for (CSButton *button in @[_apply, _back]) {
        UIButtonConfiguration *cfg = button.configuration; cfg.baseBackgroundColor = cs_color(0.20); cfg.baseForegroundColor = cs_color(1); button.configuration = cfg;
    }
#else
    _apply.layer.backgroundColor = cs_color(_apply.enabled ? 0.20 : 0.14).CGColor;
    _apply.contentTintColor = _apply.enabled ? cs_color(1) : cs_color(0.6);
    _back.layer.backgroundColor = cs_color(0.20).CGColor;
#endif
    NSString *signature = [NSString stringWithFormat:@"%d %p %@ %@", _page, pad, texts, enabled];
    if (![_signature isEqual:signature]) {
        [self glyph:_select symbol:pad.buttonA.sfSymbolsName fallback:@"a.circle"];
        [self glyph:_back symbol:pad.buttonB.sfSymbolsName fallback:@"b.circle"]; [self glyph:_apply symbol:pad.buttonY.sfSymbolsName fallback:@"y.circle"];
        [self glyph:_identify symbol:pad.buttonX.sfSymbolsName fallback:@"x.circle"];
        _signature = signature; for (CSButton *b in _rows) [b removeFromSuperview]; [_rows removeAllObjects]; _values = values;
        for (int i = 0; i < (int)texts.count; i++) {
            CSButton *b = [self button:texts[i] action:_page >= 3 ? @selector(focusDevice:) : @selector(choose:) tile:_page <= 2];
            b.tag = i; b.enabled = enabled[i].boolValue;
            if (_page <= 2) {
                NSArray<NSString *> *lines = [texts[i] componentsSeparatedByString:@"\n"];
                int style = _page == 0 ? cs_decode(cs_get(_draft, values[i].intValue) ?: _status.automatic[values[i].intValue]).style : _page == 1 ? values[i].intValue : CS_PRO;
                NSArray<NSString *> *symbols = cs_style_symbols(style);
                NSString *symbol = symbols[0];
                NSString *second = symbols.count > 1 ? symbols[1] : _page == 2 && values[i].intValue ? @"gamecontroller" : nil;
                [(DmSetupTile *)b heading:lines[0] detail:lines.count > 1 ? lines[1] : @"" status:lines.count > 2 ? lines[2] : @"" symbol:symbol second:second];
            } else [self glyph:b symbol:@"gamecontroller" fallback:@"gamecontroller"];
            [_content addSubview:b]; [_rows addObject:b];
        }
        if (_focus >= (int)_rows.count) _focus = 0;
        if (_page == 1 && _rows.count && !_rows[_focus].enabled) [self move:1];
        [self focus];
    }
    [self layout];
}
- (void)layout {
    CGRect bounds = _view.bounds;
#if TARGET_OS_IPHONE
    UIEdgeInsets safe = _view.safeAreaInsets; bounds = UIEdgeInsetsInsetRect(bounds, safe);
#endif
    CGFloat w = MIN(900, bounds.size.width - 24), h = MIN(650, bounds.size.height - 24);
    _card.frame = CGRectMake(CGRectGetMidX(bounds) - w / 2, CGRectGetMidY(bounds) - h / 2, w, h);
    CGFloat inset = w < 500 ? 18 : 28;
    BOOL compact = w < 600;
#if TARGET_OS_IPHONE
    _title.font = [UIFont systemFontOfSize:compact ? 20 : 27 weight:UIFontWeightSemibold];
#else
    _title.font = [NSFont systemFontOfSize:compact ? 20 : 27 weight:NSFontWeightSemibold];
#endif
    BOOL stacked = _page == 0 && w < 760;
    _title.frame = CGRectMake(inset, stacked ? 52 : 20, w - inset * 2, 36);
    for (int p = 0; p < (int)_people.count; p++) {
        _people[p].hidden = _page != 0;
#if TARGET_OS_IPHONE
        _people[p].alpha = (_status.ready >> p & 1) ? 1 : 0.25;
#else
        _people[p].alphaValue = (_status.ready >> p & 1) ? 1 : 0.25;
#endif
        _people[p].frame = CGRectMake(inset + p * 21, 24, 18, 22);
    }
    CGFloat top = stacked ? 104 : 76;
    _scroll.frame = CGRectMake(inset, top, w - inset * 2, MAX(44, h - top - (_identify.hidden ? 90 : 154)));
    int cols = _page == 0 || _page == 1 || _page == 2 ? 2 : 1;
    if (w < 400) cols = 1;
#if TARGET_OS_IPHONE
    CGFloat contentW = _scroll.bounds.size.width;
#else
    CGFloat contentW = _scroll.contentSize.width;
#endif
    CGFloat tileW = (contentW - (cols - 1) * 12) / cols;
    CGFloat tileH = _page == 0 ? 210 : _page == 1 || _page == 2 ? 162 : 68;
    int rowCount = ((int)_rows.count + cols - 1) / cols;
    _content.frame = CGRectMake(0, 0, contentW, MAX(_scroll.bounds.size.height, rowCount * (tileH + 12)));
#if TARGET_OS_IPHONE
    _scroll.contentSize = _content.bounds.size;
#endif
    for (int i = 0; i < (int)_rows.count; i++) {
        _rows[i].frame = CGRectMake((i % cols) * (tileW + 12), (i / cols) * (tileH + 12), tileW, tileH);
        if ([_rows[i] isKindOfClass:DmSetupTile.class]) [(DmSetupTile *)_rows[i] arrange];
    }
    CGFloat bw = (w - inset * 2 - 24) / 3;
    _apply.frame = CGRectMake(inset, h - 74, bw, 48); _select.frame = CGRectMake(inset + bw + 12, h - 74, bw, 48); _back.frame = CGRectMake(w - inset - bw, h - 74, bw, 48);
    _identify.frame = CGRectMake((w - 150) / 2, h - 128, 150, 40);
    [self presentHoldAt:NSProcessInfo.processInfo.systemUptime];
}
- (void)tick {
    if (!_view) return;
    unsigned buttons = self.buttons() | (unsigned)_keys | (_pointerHeld ? CSU_APPLY : 0) | (_pointerBack ? CSU_BACK : 0);
    if (_settling) {
        if (buttons) return;
        [_timer invalidate]; _timer = nil; [_view removeFromSuperview]; _view = nil;
        self.finished(_ok); return;
    }
    [self refresh];
    double now = NSProcessInfo.processInfo.systemUptime;
    unsigned pressed = buttons & ~(unsigned)_previous;
    if (!(buttons & CSU_BACK) && (pressed & CSU_SELECT)) { if (_focus < (int)_rows.count) [self choose:_rows[_focus]]; }
    else if (!(buttons & CSU_BACK) && (pressed & CSU_IDENTIFY)) [self identify:nil];
    unsigned directions = buttons & (CSU_UP | CSU_DOWN | CSU_LEFT | CSU_RIGHT);
    if (!(buttons & CSU_BACK) && directions && (pressed & directions || now >= _repeatAt)) {
        int columns = _page <= 2 && _card.bounds.size.width >= 400 ? 2 : 1;
        int delta = directions & (CSU_UP | CSU_DOWN) ? columns : 1;
        [self move:directions & (CSU_UP | CSU_LEFT) ? -delta : delta]; _repeatAt = now + (pressed & directions ? 0.35 : 0.12);
    }
    BOOL active;
#if TARGET_OS_IPHONE
    active = UIApplication.sharedApplication.applicationState == UIApplicationStateActive;
#else
    active = NSApp.isActive && _view.window.isKeyWindow;
#endif
    [self advanceHolds:buttons active:active at:now];
    _previous = (int)buttons;
}
@end
#undef CSView
#undef CSButton
#undef CSLabel
#undef CSColor
#undef CSScroll

#undef CSImageView
