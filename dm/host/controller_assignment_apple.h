// The Apple device-swap adapter, independent of its window. Tests use fake
// devices with the same methods, while the host supplies its actual slot table and callbacks.
#pragma once
#include "controller_routes_ios.h"
#include "pad_slots.h"
#include "pad_apple.h"
#include "motion_apple.h"
#include "haptics_apple.h"

typedef struct {
    pad_slots *slots;
    GCController * __strong *held;
    int *merge, *keyboardOwn;
    NSArray<GCController *> *(*devices)(void);
    int (*mode)(void);
    void (*read)(GCExtendedGamepad *);
    void (*keep)(GCController *);
    void (*changed)(GCController *);
} dm_controller_host;

static NSDictionary *controller_assignment_state(dm_controller_host host) {
    NSArray *rows = cs_update(host.devices());
    NSMutableArray *choices = [NSMutableArray new];
    for (NSDictionary *row in rows) {
        NSString *key = row[@"id"];
        NSDictionary *choice = cs_choices[key] ?: @{@"id": key, @"player": @(-1), @"style": @"auto"};
        [choices addObject:choice];
    }
    return @{@"controllers": rows, @"choices": choices};
}
static dm_ios_routes controller_assignment_plan(dm_controller_host host,NSArray *choices,
        NSArray **outRows,NSArray **outSources,NSDictionary **outOverrides) {
    NSArray *rows = cs_update(host.devices());
    NSMutableDictionary *overrides = [NSMutableDictionary new];
    for (NSDictionary *choice in choices)
        if ([choice[@"player"] intValue] != -1 || ![choice[@"style"] isEqual:@"auto"]) overrides[choice[@"id"]] = choice;
    NSMutableArray *sources = [NSMutableArray new];
    for (int p = 0; p < DM_PLAYERS; p++) [sources addObject:[NSMutableArray new]];
    uint32_t reserved = 0;
    for (int p = 0; p < DM_PLAYERS; p++) if (host.slots->owner[p] && !host.held[p]) reserved |= 1u << p;
    for (NSDictionary *choice in overrides.allValues) {
        int p = [choice[@"player"] intValue] - 1;
        if (p >= 0) reserved &= ~(1u << p);
    }
    dm_ios_routes plan = ios_plan_routes_reserved(*host.slots, rows, overrides, *host.merge && !overrides.count, sources, reserved);
    if (outRows) *outRows=rows;
    if (outSources) *outSources=sources;
    if (outOverrides) *outOverrides=overrides;
    return plan;
}
static dm_controller_evaluation controller_assignment_evaluate(dm_controller_host host,NSArray *choices) {
    NSDictionary *overrides=nil;
    dm_ios_routes plan=controller_assignment_plan(host,choices,NULL,NULL,&overrides);
    dm_controller_routes routes={.occupied=pad_slot_mask(&plan.slots),.explicit_routes=cs_explicit || overrides.count!=0,
        .handheld=!host.mode(),.fallback=1};
    dm_controller_routes live=dm_controller_current_routes();
    for (int p=0;p<DM_PLAYERS;p++) {
        BOOL reserved=plan.slots.owner[p] && !host.held[p] && plan.slots.owner[p]==host.slots->owner[p];
        routes.styles[p]=reserved ? live.styles[p] : plan.styles[p];
        routes.sides[p]=reserved ? live.sides[p] : plan.sides[p];
        if (reserved ? (live.grouped>>p&1u) : plan.sides[p]!=0) routes.grouped|=1u<<p;
    }
    dm_controller_limits limits;
    dm_controller_setup_requirements(&limits.styles,&limits.ids,&limits.minimum,&limits.maximum);
    return dm_controller_evaluate(routes,limits);
}
static NSArray *controller_assignment_resolve(dm_controller_host host, NSArray *choices) {
    NSArray *rows=nil,*sources=nil; NSDictionary *overrides=nil;
    dm_ios_routes plan=controller_assignment_plan(host,choices,&rows,&sources,&overrides);
    dm_controller_evaluation evaluation=controller_assignment_evaluate(host,choices);
    NSMutableArray *resolved = [NSMutableArray new];
    for (NSDictionary *row in rows) {
        NSString *key = row[@"id"]; GCController *device = cs_devices[key];
        NSDictionary *choice = overrides[key];
        int slot = -1;
        for (int p = 0; p < DM_PLAYERS; p++) if ([sources[p] containsObject:device]) { slot = p; break; }
        uint32_t configured = choice ? cs_style(choice[@"style"]) : 0;
        NSString *automatic = slot == 0 && !host.mode() ? @"handheld" : @"pro";
        [resolved addObject:@{@"id": key, @"player": @(slot + 1),
            @"style": configured ? cs_style_name(configured) : automatic,
            @"configuredStyle": choice[@"style"] ?: @"auto", @"automaticStyle": automatic,
            @"ready": @(slot>=0 && (evaluation.ready>>slot&1u)),
            @"rejected": @(slot>=0 ? evaluation.rejected[slot] : DM_CONTROLLER_ABSENT),
            @"role": choice[@"role"] ?: @"both", @"pairComplete": @(slot<0 || !plan.sides[slot] || plan.sides[slot]==3)}];
    }
    return resolved;
}
static dm_controller_requirements controller_assignment_requirements(dm_controller_host host) {
    dm_controller_requirements game;
    dm_controller_setup_requirements(&game.styles, &game.ids, NULL, NULL);
    int docked = host.mode();
    BOOL supported = (game.styles & (docked ? DM_STYLE_PRO : 2u)) && (game.ids & (docked ? 1u : 1u << 8));
    game.fallback = supported ? DM_FALLBACK_IF_PLAYER_ONE_FREE : DM_FALLBACK_NONE;
    return game;
}
static GCController *cs_pair_left[DM_PLAYERS], *cs_pair_right[DM_PLAYERS];
static BOOL cs_pair_reconciling;
static NSString *controller_assignment_apply(dm_controller_host host, NSArray *choices, int minimum, int maximum, uint32_t *changed) {
    cs_update(host.devices());
    NSDictionary *next = nil; NSString *error = cs_validate_choices(choices, &next);
    if (error) return error;
    error=cs_pair_error(choices); if (error && !cs_pair_reconciling) return error;
    dm_pad_reconfigure_begin();
    @try {
        int low=0,high=DM_PLAYERS; dm_controller_setup_requirements(NULL,NULL,&low,&high);
        minimum=MAX(minimum,low); maximum=MIN(maximum,high);
        NSArray *resolved = controller_assignment_resolve(host, choices);
        int count=dm_controller_capped_count(controller_assignment_evaluate(host,choices).ready_count,maximum);
        // A physical disconnect must be published even while the game waits for more players.
        if (!cs_pair_reconciling && (minimum>maximum || count < minimum)) return @"Assign the number of supported controllers this game requests.";
        BOOL edited = NO;
        for (NSDictionary *choice in next.allValues)
            if ([choice[@"player"] intValue] != -1 || ![choice[@"style"] isEqual:@"auto"]) edited = YES;
        if (!edited && !cs_explicit) { if (changed) *changed = 0; return nil; }
        GCController *left[DM_PLAYERS]={nil}, *right[DM_PLAYERS]={nil};
        GCController *owners[DM_PLAYERS] = {nil}; uint32_t styles[DM_PLAYERS] = {0}, affected = 0;
        for (NSDictionary *choice in resolved) {
            int p = [choice[@"player"] intValue] - 1; if (p < 0) continue;
            GCController *c=cs_devices[choice[@"id"]]; int role=cs_role(choice);
            if (role==1) left[p]=c; else if (role==2) right[p]=c; else owners[p]=c;
            styles[p] = cs_style(choice[@"configuredStyle"]);
        }
        for (int p=0;p<DM_PLAYERS;p++) if (left[p] || right[p]) owners[p]=left[p] ?: right[p];
        for (int p = 0; p < DM_PLAYERS; p++) {
            // Keyboard and raw-HID slots are outside this device list. An
            // Automatic choice must not displace a source the sheet cannot edit.
            if (host.slots->owner[p] && !host.held[p] && !owners[p]) continue;
            if (cs_pair_left[p]!=left[p] || cs_pair_right[p]!=right[p] || *host.merge || host.slots->owner[p] != (__bridge const void *)owners[p] || dm_pad_configured_style(p) != styles[p]) affected |= 1u << p;
        }
        // Legacy player 1 can still have a queued controller sample after its device
        // disconnected. Entering explicit mode must invalidate that source as well.
        if (!cs_explicit) affected |= 1u;
        dm_pad_reconfigure_reset(affected);
        // Invalidate native routes before any new owner is attached. Queued gyro samples
        // carry a generation and cannot write back into a slot after its owner changes.
        if (*host.merge) for (GCController *c in host.devices()) {
            c.extendedGamepad.valueChangedHandler = nil; dm_motion_detach(c, 0);
        }
        for (int p = 0; p < DM_PLAYERS; p++) if (affected >> p & 1) {
            if (cs_pair_left[p]) dm_motion_detach_role(cs_pair_left[p],p,1);
            if (cs_pair_right[p]) { cs_pair_right[p].extendedGamepad.valueChangedHandler=nil; dm_motion_detach_role(cs_pair_right[p],p,2); }
            if (host.held[p]) { host.held[p].extendedGamepad.valueChangedHandler = nil; dm_motion_detach(host.held[p], p); }
            dm_haptics_apple_controller(p, nil); host.held[p] = nil;
            pad_slot_free(host.slots, host.slots->owner[p]);
        }
        cs_choices = next; cs_explicit = 1; *host.merge = 0;
        if (host.keyboardOwn && owners[0]) *host.keyboardOwn = 0;
        dm_pad_explicit(1);
        for (int p = 0; p < DM_PLAYERS; p++) {
            if (host.slots->owner[p] && !host.held[p] && !owners[p]) continue;
            dm_pad_style(p, styles[p], owners[p] && !dm_pad_is_joycon(owners[p]));
            uint32_t sides=(left[p] ? 1u : 0u)|(right[p] ? 2u : 0u);
            dm_pad_pair_sources(p,sides!=0,sides);
            if (!(affected >> p & 1)) continue;
            cs_pair_left[p]=left[p]; cs_pair_right[p]=right[p];
            if (!owners[p]) continue;
            GCController *c = owners[p];
            pad_slot_assign(host.slots, (__bridge const void *)c, (c.vendorName.UTF8String ?: "?"), p); host.held[p] = c;
            cs_admit(c); host.keep(c);
            void (*read)(GCExtendedGamepad *) = host.read;
            c.extendedGamepad.valueChangedHandler = ^(GCExtendedGamepad *g, GCControllerElement *el) { (void)el; read(g); };
            cs_pair_left[p]=left[p]; cs_pair_right[p]=right[p];
            if (sides) {
                for (int role=1;role<=2;role++) {
                    GCController *side=role==1 ? left[p] : right[p]; if (!side) continue;
                    cs_admit(side); host.keep(side);
                    side.extendedGamepad.valueChangedHandler=^(GCExtendedGamepad *g,GCControllerElement *el) { (void)el; read(g); };
                    dm_motion_attach_role(side,p,role);
                }
                dm_haptics_apple_pair(p,left[p],right[p]);
            } else { dm_motion_attach(c, p); dm_haptics_apple_controller(p, c); }
        }
        dm_pad_players(pad_slot_mask(host.slots));
        if (host.changed) host.changed(nil);
        if (changed) *changed = affected;
        return nil;
    } @finally { dm_pad_reconfigure_end(); }
}
