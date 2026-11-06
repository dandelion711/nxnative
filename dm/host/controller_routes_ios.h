// Plan both automatic routes and per-device overrides before touching live input.
#pragma once
#include "controller_setup_apple.h"
#include "pad_slots.h"

typedef struct {
    pad_slots slots;
    uint32_t styles[DM_PLAYERS];
    uint32_t manual;
    uint32_t sides[DM_PLAYERS];
} dm_ios_routes;

static uint32_t ios_style(NSString *name) {
    return [name isEqual:@"auto"] ? DM_STYLE_AUTO : cs_style(name);
}

static NSString *ios_validate_overrides(NSArray *choices, NSDictionary **accepted) {
    NSDictionary *validated=nil;
    NSString *error=cs_validate_choices(choices,&validated); if (error) return error;
    NSMutableDictionary *overrides=[NSMutableDictionary new];
    for (NSString *key in validated) {
        NSDictionary *c=validated[key];
        if ([c[@"player"] intValue]!=-1 || ![c[@"style"] isEqual:@"auto"]) overrides[key]=c;
    }
    *accepted=[overrides copy]; return nil;
}

static void ios_route(dm_ios_routes *plan, NSArray<NSMutableArray<GCController *> *> *sources,
                      id controller, NSString *name, int slot, NSDictionary *choice) {
    if (slot < 0) return;
    if (!sources[slot].count)
        pad_slot_assign(&plan->slots, (__bridge const void *)controller, name.UTF8String ?: "", slot);
    [sources[slot] addObject:controller];
    if (choice) {
        plan->styles[slot] = ios_style(choice[@"style"]);
        plan->manual |= 1u << slot;
        int role=cs_role(choice); if (role) plan->sides[slot] |= 1u << (role-1);
    }
}

static dm_ios_routes ios_plan_routes_reserved(pad_slots previous, NSArray *rows, NSDictionary *choices, BOOL merge,
                                     NSArray<NSMutableArray<GCController *> *> *sources, uint32_t reserved) {
    dm_ios_routes plan = {.slots = previous};
    for (int p = 0; p < DM_PLAYERS; p++) if (!(reserved >> p & 1)) plan.slots.owner[p] = NULL;

    // Fixed slots take precedence, including when they displace an automatic pad.
    for (NSDictionary *row in rows) {
        NSString *key = row[@"id"];
        NSDictionary *choice = choices[key];
        int player = [choice[@"player"] intValue];
        if (player > 0) ios_route(&plan, sources, cs_devices[key], row[@"name"], player - 1, choice);
    }

    // Retain existing slots for connected devices. In legacy merge mode, the
    // automatic group is rebuilt together after any manual slots are reserved.
    for (NSDictionary *row in rows) {
        NSString *key = row[@"id"];
        NSDictionary *choice = choices[key];
        if ((choice && [choice[@"player"] intValue] != -1) || (merge && !choice && !(plan.manual & 1u))) continue;
        id controller = cs_devices[key];
        int slot = pad_slot_of(&previous, (__bridge const void *)controller);
        if (slot >= 0 && !plan.slots.owner[slot]) ios_route(&plan, sources, controller, row[@"name"], slot, choice);
    }

    // Devices with a type override and an automatic player need their own slot.
    // Completely automatic pads follow connection order and remembered slots.
    for (NSDictionary *row in rows) {
        NSString *key = row[@"id"];
        NSDictionary *choice = choices[key];
        if (choice && [choice[@"player"] intValue] != -1) continue;
        id controller = cs_devices[key];
        const void *owner = (__bridge const void *)controller;
        if (pad_slot_of(&plan.slots, owner) >= 0) continue;
        if (merge && !choice && !(plan.manual & 1u)) {
            ios_route(&plan, sources, controller, row[@"name"], 0, nil);
        } else {
            int slot = pad_slot_take(&plan.slots, owner, [row[@"name"] UTF8String] ?: "");
            ios_route(&plan, sources, controller, row[@"name"], slot, choice);
        }
    }
    return plan;
}

static dm_ios_routes ios_plan_routes(pad_slots previous, NSArray *rows, NSDictionary *choices, BOOL merge,
                                     NSArray<NSMutableArray<GCController *> *> *sources) {
    return ios_plan_routes_reserved(previous, rows, choices, merge, sources, 0);
}
