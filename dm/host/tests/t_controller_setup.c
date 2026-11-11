#include "../controller_setup.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    for (int p = 0; p < 8; p++) for (int a = 0; a < 8; a++) for (int b = 0; b < 8; b++) {
        unsigned code = cs_encode(CS_PAIR, a, b); cs_route route = cs_decode(code);
        assert(route.style == CS_PAIR && route.first == a && route.second == b);
        uint64_t bits = cs_put(0, p, code); assert(cs_get(bits, p) == code);
        for (int q = 0; q < 8; q++) if (q != p) assert(cs_get(bits, q) == 0);
    }
    cs_hold hold = {0};
    assert(!cs_hold_step(&hold, 1, 1, 1)); // opening while held never applies
    assert(!cs_hold_step(&hold, 0, 1, 2));
    assert(!cs_hold_step(&hold, 1, 1, 3));
    assert(!cs_hold_step(&hold, 1, 1, 3.79));
    assert(cs_hold_step(&hold, 1, 1, 3.81));
    assert(!cs_hold_step(&hold, 1, 1, 5)); // once per hold
    cs_hold_step(&hold, 0, 1, 6); cs_hold_step(&hold, 1, 1, 7);
    assert(!cs_hold_step(&hold, 1, 0, 8)); // unplugged / not ready resets
    assert(!cs_hold_step(&hold, 1, 1, 8.1));
    assert(!cs_hold_step(&hold, 1, 1, 8.8));
    assert(!cs_hold_step(&hold, 1, 1, 9));
    cs_hold_step(&hold, 0, 1, 10); cs_hold_step(&hold, 1, 1, 11);
    assert(cs_hold_step(&hold, 1, 1, 12));
    cs_status status = {.minimum = 1, .maximum = 1, .ready = 1}; assert(cs_can_apply(status));
    status.invalid = 2; assert(!cs_can_apply(status));
    status.invalid = 0; status.ready = 3; assert(!cs_can_apply(status));
    puts("controller setup: route encoding, hold, readiness passed");
}
