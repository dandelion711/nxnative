// Controller setup's pending routes and hold-to-apply. No platform objects; the HID and UI tests use the same rules.
#pragma once
#include <stdint.h>
#include "dm_input.h"
enum { CS_AUTO, CS_OFF, CS_PRO, CS_LEFT, CS_RIGHT, CS_PAIR };
typedef struct { int style, first, second; } cs_route;
// One byte per player makes all eight assignments one atomic publication. Equal pair sources mean one gamepad drives both halves.
static inline unsigned cs_encode(int style, int a, int b) {
    if (style == CS_AUTO || style == CS_OFF) return (unsigned)style;
    if (style < CS_PRO || style > CS_PAIR) return CS_OFF;
    if (a < 0 || a >= DM_PLAYERS || b < 0 || b >= DM_PLAYERS) return CS_OFF;
    return style == CS_PAIR ? 26u + (unsigned)(a * 8 + b) : 2u + (unsigned)((style - CS_PRO) * 8 + a);
}
static inline cs_route cs_decode(unsigned c) {
    if (c < 2) return (cs_route){(int)c, -1, -1};
    if (c < 26) return (cs_route){CS_PRO + (int)((c - 2) / 8), (int)((c - 2) % 8), (int)((c - 2) % 8)};
    if (c < 90) return (cs_route){CS_PAIR, (int)((c - 26) / 8), (int)((c - 26) % 8)};
    return (cs_route){CS_OFF, -1, -1};
}
static inline unsigned cs_get(uint64_t routes, int p) { return (unsigned)(routes >> (p * 8) & 255); }
static inline uint64_t cs_put(uint64_t routes, int p, unsigned c) { return (routes & ~(255ull << (p * 8))) | (uint64_t)c << (p * 8); }
static inline uint32_t cs_sources(cs_route r) { return r.first < 0 ? 0 : (1u << r.first) | (1u << r.second); }
static inline uint32_t cs_style(int style) { return style == CS_PRO ? 1u : style == CS_PAIR ? 4u : style == CS_LEFT ? 8u : style == CS_RIGHT ? 16u : 0; }
typedef struct {
    int minimum, maximum;
    uint32_t allowed, raw_allowed, ids, sources, ready, invalid;
    uint32_t native_sides[DM_PLAYERS];
    unsigned automatic[DM_PLAYERS]; // effective upstream route, including a split pair's other half
} cs_status;
static inline int cs_can_apply(cs_status s) {
    return !s.invalid && __builtin_popcount(s.ready) >= s.minimum && __builtin_popcount(s.ready) <= s.maximum;
}
// A hold starts only after release, resets on navigation / incompatibility / focus loss, and fires once.
typedef struct { double since; int armed, down; } cs_hold;
static inline int cs_hold_step(cs_hold *h, int down, int valid, double now) {
    if (!down) { *h = (cs_hold){.armed = 1}; return 0; }
    if (!valid) { *h = (cs_hold){0}; return 0; }
    if (!h->armed) return 0;
    if (!h->down) { h->since = now; h->down = 1; }
    if (now - h->since < 0.8) return 0;
    *h = (cs_hold){0}; return 1;
}

uint64_t dm_controller_routes(void);
void dm_controller_forget_source(int source); // mark disconnected before replacing an owner in the same physical slot
cs_status dm_controller_preview(uint64_t routes, int minimum, int maximum, int permit_pair);
int dm_controller_apply(uint64_t routes, int minimum, int maximum, int permit_pair);
void dm_controller_request(int *minimum, int *maximum, int *permit_pair);

void dm_controller_native_joy(int source, int native); // GameController Joy-Cons retain their native orientation

void dm_controller_source_name(int source, const char *name); // upstream slot/model matching restores reconnects; other models fall back to Automatic

// Call off the UI thread; an active game request or editor keeps ownership and returns 0.
int dm_controller_configure(void);

void dm_controller_configure_startup(void); // one-shot launch-screen request, before guest code starts
