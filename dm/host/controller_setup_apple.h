// The Apple frontends use session IDs to match choices to connected devices.
#pragma once
#import <GameController/GameController.h>
#include "controller_requirements_apple.h"

static NSMutableDictionary<NSString *, GCController *> *cs_devices;
static NSDictionary<NSString *, NSDictionary *> *cs_choices;
static int cs_explicit;

static NSArray *cs_update(NSArray<GCController *> *devices) {
    if (!cs_devices) cs_devices = [NSMutableDictionary new];
    NSMutableSet *seen = [NSMutableSet new];
    NSMutableArray *rows = [NSMutableArray new];
    for (GCController *device in devices) if (device.extendedGamepad) {
        NSString *key = [cs_devices allKeysForObject:device].firstObject ?: NSUUID.UUID.UUIDString;
        cs_devices[key] = device; [seen addObject:key];
        [rows addObject:@{@"id": key, @"name": device.vendorName ?: @"Controller"}];
    }
    for (NSString *key in cs_devices.allKeys) if (![seen containsObject:key]) [cs_devices removeObjectForKey:key];
    return rows;
}

static int cs_device_role(GCController *c, NSDictionary *choices) {
    NSString *key=[cs_devices allKeysForObject:c].firstObject;
    return key ? cs_role(choices[key]) : 0;
}
// Shared by startup and live Apply. Neither UI may partially publish an invalid draft.
static NSString *cs_validate_choices(NSArray *choices, NSDictionary **accepted) {
    if (![choices isKindOfClass:NSArray.class]) return @"Invalid controller choices.";
    NSMutableDictionary *next = [NSMutableDictionary new];
    NSMutableDictionary *players = [NSMutableDictionary new];
    for (id row in choices) {
        if (![row isKindOfClass:NSDictionary.class]) return @"Invalid controller choice.";
        NSString *key = row[@"id"], *style = row[@"style"];
        NSNumber *player = row[@"player"];
        if (![key isKindOfClass:NSString.class] || ![style isKindOfClass:NSString.class] || ![player isKindOfClass:NSNumber.class] ||
            player.doubleValue != player.intValue || player.intValue < -1 || player.intValue > DM_PLAYERS || (!cs_style(style) && ![style isEqual:@"auto"]) || next[key]) return @"Invalid controller assignment.";
        id role = row[@"role"];
        if (role && (![role isKindOfClass:NSString.class] || ![@[@"both",@"left",@"right"] containsObject:role])) return @"Invalid pair role.";
        if (cs_role(row) && (![style isEqual:@"pair"] || player.intValue <= 0)) return @"Choose a player and Joy-Con pair before assigning a pair role.";
        if (!cs_devices[key]) return @"A controller disconnected. Choose a connected controller and try again.";
        if (player.intValue > 0) {
            NSArray *group=players[player] ?: @[];
            if (group.count && (group.count!=1 || !cs_can_share(group[0],row))) return @"A shared player needs one left and one right controller set to Joy-Con pair.";
            players[player]=[group arrayByAddingObject:row];
        }
        next[key] = [row copy];
    }
    if (accepted) *accepted = [next copy];
    return nil;
}

static NSString *cs_style_name(uint32_t style) {
    return style == DM_STYLE_PAIR ? @"pair" : style == DM_STYLE_LEFT ? @"left" : style == DM_STYLE_RIGHT ? @"right" : @"pro";
}

static int cs_player(GCController *controller, uint32_t *style) {
    NSString *key = [cs_devices allKeysForObject:controller].firstObject;
    NSDictionary *choice = key ? cs_choices[key] : nil;
    int player = [choice[@"player"] intValue];
    if (style) *style = cs_style(choice[@"style"]);
    return !choice || player == -1 ? -2 : player ? player - 1 : -1;
}

typedef BOOL (*cs_identify_fn)(GCController *);

static BOOL cs_identify(NSString *key, cs_identify_fn signal) {
    GCController *device = cs_devices[key];
    return device && signal && signal(device);
}
