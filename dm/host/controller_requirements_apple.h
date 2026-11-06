// The same game acceptance rules drive the controller sheet and live Apply.
#pragma once
#import <Foundation/Foundation.h>
#include "dm_input.h"

typedef enum {
    DM_FALLBACK_NONE, DM_FALLBACK_IF_PLAYER_ONE_FREE
} dm_controller_fallback;

typedef struct {
    uint32_t styles, ids;
    dm_controller_fallback fallback;
} dm_controller_requirements;

static inline uint32_t cs_style(NSString *name) {
    if ([name isEqual:@"handheld"]) return 2u;
    if ([name isEqual:@"pro"]) return DM_STYLE_PRO;
    if ([name isEqual:@"pair"]) return DM_STYLE_PAIR;
    if ([name isEqual:@"left"]) return DM_STYLE_LEFT;
    if ([name isEqual:@"right"]) return DM_STYLE_RIGHT;
    return 0;
}

static inline BOOL controller_player_supported(dm_controller_requirements game, int player) {
    return player > 0 && player <= DM_PLAYERS && (game.ids & (1u << (player - 1)));
}

static inline int controller_ready_players(dm_controller_requirements game, NSArray *choices, BOOL *fallbackCounts) {
    int count = 0; uint32_t counted = 0;
    BOOL firstAssigned = NO;
    for (NSDictionary *choice in choices) {
        int player = [choice[@"player"] intValue];
        if (player == 1) firstAssigned = YES;
        if (choice[@"pairComplete"] && ![choice[@"pairComplete"] boolValue]) continue;
        BOOL slotSupported = player == 1 && [choice[@"style"] isEqual:@"handheld"]
            ? (game.ids & (1u << 8)) != 0 : controller_player_supported(game, player);
        if (slotSupported && (game.styles & cs_style(choice[@"style"])) && !(counted >> (player - 1) & 1)) {
            counted |= 1u << (player - 1); count++;
        }
    }
    BOOL fallback = game.fallback == DM_FALLBACK_IF_PLAYER_ONE_FREE && !firstAssigned;
    if (fallbackCounts) *fallbackCounts = fallback;
    return count + fallback;
}

// A recommendation only changes the draft. Prefer full controllers when
// accepted, otherwise alternate the accepted single Joy-Con sides by player.
static inline NSString *controller_recommended_style(uint32_t supported, int player) {
    if (supported & DM_STYLE_PRO) return @"pro";
    if (supported & DM_STYLE_PAIR) return @"pair";
    if ((supported & (DM_STYLE_LEFT | DM_STYLE_RIGHT)) == (DM_STYLE_LEFT | DM_STYLE_RIGHT))
        return player % 2 ? @"right" : @"left";
    if (supported & DM_STYLE_LEFT) return @"left";
    if (supported & DM_STYLE_RIGHT) return @"right";
    return nil;
}

// The badge names a sole accepted type. A suggested draft may still choose
// between several accepted types, and does not imply physical sensor support.
static inline NSString *controller_required_style(uint32_t supported) {
    if (!supported || (supported & (supported - 1u))) return nil;
    return controller_recommended_style(supported, 0);
}

static inline BOOL controller_style_recommended(uint32_t supported, uint32_t style) {
    return (supported == (DM_STYLE_LEFT|DM_STYLE_RIGHT) && (style==DM_STYLE_LEFT || style==DM_STYLE_RIGHT)) ||
        (style && supported==style && controller_required_style(supported)!=nil);
}

static int cs_role(NSDictionary *choice) {
    return [choice[@"role"] isEqual:@"left"] ? 1 : [choice[@"role"] isEqual:@"right"] ? 2 : 0;
}
static BOOL cs_can_share(NSDictionary *a, NSDictionary *b) {
    return [a[@"style"] isEqual:@"pair"] && [b[@"style"] isEqual:@"pair"] && cs_role(a) && cs_role(b) && cs_role(a)!=cs_role(b);
}
static NSString *cs_pair_error(NSArray *choices) {
    uint32_t sides[DM_PLAYERS]={0};
    for (NSDictionary *c in choices) {
        int p=[c[@"player"] intValue]-1, role=cs_role(c);
        if (p>=0 && p<DM_PLAYERS && role) sides[p] |= 1u << (role-1);
    }
    for (int p=0;p<DM_PLAYERS;p++) if (sides[p] && sides[p]!=3)
        return @"Assign a left and a right controller to the same player to complete the pair.";
    return nil;
}

static inline int controller_player_capacity(dm_controller_requirements game) {
    uint32_t external=game.styles & (DM_STYLE_PRO|DM_STYLE_PAIR|DM_STYLE_LEFT|DM_STYLE_RIGHT);
    int n=external ? __builtin_popcount(game.ids & 0xffu) : 0;
    if ((game.styles & 2u) && (game.ids & (1u<<8)) && (!external || !(game.ids & 1u))) n++;
    return MIN(DM_PLAYERS,n);
}
// Plan only: preserve complete pair groups and device order within a player.
// Apply still validates real ready players against the game's current limits.
static NSArray *controller_choices_for_count(NSArray *choices, NSArray *resolved, int wanted, uint32_t ids) {
    NSMutableDictionary *routes=[NSMutableDictionary new], *groups=[NSMutableDictionary new];
    NSMutableArray *ordered=[NSMutableArray new];
    for (NSDictionary *r in resolved) routes[r[@"id"]]=r;
    for (NSDictionary *c in choices) {
        NSString *key=c[@"id"]; NSDictionary *route=routes[key]; if (!route) continue;
        int player=[route[@"player"] intValue];
        NSString *group=cs_role(c) && [c[@"player"] intValue]>0 ? [NSString stringWithFormat:@"pair.%@",c[@"player"]] : key;
        if (!groups[group]) { groups[group]=[NSMutableArray new]; [ordered addObject:@{@"key":group,@"player":@(player>0 ? player : DM_PLAYERS+1),@"order":@(ordered.count)}]; }
        [groups[group] addObject:c];
    }
    [ordered sortUsingComparator:^NSComparisonResult(NSDictionary *a,NSDictionary *b) { NSComparisonResult byPlayer=[a[@"player"] compare:b[@"player"]]; return byPlayer==NSOrderedSame ? [a[@"order"] compare:b[@"order"]] : byPlayer; }];
    NSMutableArray *slots=[NSMutableArray new];
    for (int p=0;p<DM_PLAYERS;p++) if (ids>>p&1) [slots addObject:@(p+1)];
    if (!slots.count && (ids&(1u<<8))) [slots addObject:@1];
    NSMutableDictionary *next=[NSMutableDictionary new]; int n=0;
    for (NSDictionary *g in ordered) {
        NSArray *members=groups[g[@"key"]];
        BOOL take=n<wanted && n<(int)slots.count;
        for (NSDictionary *c in members) {
            NSMutableDictionary *row=[c mutableCopy]; row[@"player"]=take ? slots[n] : @0;
            if (!take) row[@"role"]=@"both";
            next[row[@"id"]]=row;
        }
        if (take) n++;
    }
    NSMutableArray *result=[NSMutableArray new];
    for (NSDictionary *c in choices) [result addObject:next[c[@"id"]] ?: c];
    return result;
}
