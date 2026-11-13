"""Exercise the production reset entry point with recorded input sinks on macOS."""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(sys.platform == 'darwin' and shutil.which('clang'), 'needs macOS clang')
class GamepadResetTests(unittest.TestCase):
    def test_reset_releases_both_native_halves(self):
        source = (Path(__file__).resolve().parents[1] / 'Sources/DmGamepad.m').read_text()
        start = source.index('void dm_gamepad_reset(void) {')
        reset = source[start:source.index('\n}', start) + 2]
        harness = r'''
#import <Foundation/Foundation.h>
#include <pthread.h>
#include <assert.h>
#define DM_PLAYERS 8
@interface ResetView : NSObject
- (void)reset;
@end
@implementation ResetView
- (void)reset {}
@end
static ResetView *g_view;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_shake_btn, g_merge, g_held[DM_PLAYERS], merged;
static int buttons[DM_PLAYERS][2], axes[DM_PLAYERS][2], pads[DM_PLAYERS];
static void publish(void) {}
static void dm_pad_set(int b,int x,int y,int r,int s) { merged=b+x+y+r+s; }
static void dm_pad_set_player(int p,int b,int x,int y,int r,int s) { pads[p]=b+x+y+r+s; }
static void dm_joy_set(int p,int side,int b,int x,int y) { buttons[p][side]=b; axes[p][side]=x+y; }
'''
        harness += reset + r'''
int main(void) {
    g_held[0]=g_held[3]=1;
    for (int p=0;p<DM_PLAYERS;p++) { pads[p]=1; for(int s=0;s<2;s++) buttons[p][s]=axes[p][s]=1; }
    dm_gamepad_reset();
    for (int p=0;p<DM_PLAYERS;p++) {
        assert(pads[p]==!g_held[p]);
        for(int s=0;s<2;s++) { assert(buttons[p][s]==!g_held[p]); assert(axes[p][s]==!g_held[p]); }
    }
    g_merge=1; merged=1; dm_gamepad_reset(); assert(!merged);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            src, exe = Path(tmp) / 'reset.m', Path(tmp) / 'reset'
            src.write_text(harness)
            subprocess.run(['clang', '-fobjc-arc', '-Wall', '-Werror', str(src),
                            '-framework', 'Foundation', '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
