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
