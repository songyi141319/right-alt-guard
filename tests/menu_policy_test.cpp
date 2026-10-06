#include "../src/menu_policy.h"
#include <stdio.h>

static int checks = 0;
static int failures = 0;
static const uintptr_t windowA = 0x1234;
static const uintptr_t windowB = 0x5678;

static void Expect(const char* name, bool value) {
    ++checks;
    if (!value) { ++failures; printf("FAIL: %s\n", name); }
}
static void Key(MenuPolicy& policy, bool rightAlt, bool up, uint32_t time,
                uintptr_t window = windowA, bool repeat = false, bool modifiers = false) {
    ObserveKey(policy, rightAlt, up, repeat, modifiers, window, time);
}
static void Tap(MenuPolicy& policy, uint32_t time = 100, uintptr_t window = windowA) {
    Key(policy, true, false, time, window);
    Key(policy, true, true, time + 30, window);
}

int main() {
    MenuPolicy policy;
    Tap(policy);
    Expect("bare right Alt prevents native menu focus theft", BlockMenu(policy, 0xF100, 0, windowA, 131));
    Expect("one release only blocks one menu request", !BlockMenu(policy, 0xF100, 0, windowA, 132));

    policy = {}; Key(policy, false, false, 100); Key(policy, false, true, 130);
    Expect("left Alt can still open the menu", !BlockMenu(policy, 0xF100, 0, windowA, 131));
    policy = {}; Key(policy, true, false, 100); Key(policy, false, false, 110); Key(policy, true, true, 130);
    Expect("right Alt with Tab or another key is preserved", !BlockMenu(policy, 0xF100, 0, windowA, 131));
    policy = {}; Key(policy, true, false, 100, windowA, false, true); Key(policy, true, true, 130);
    Expect("a modifier already held prevents suppression", !BlockMenu(policy, 0xF100, 0, windowA, 131));

    policy = {}; Tap(policy);
    Expect("Alt F mnemonic is preserved", !BlockMenu(policy, 0xF100, 'f', windowA, 131));
    Expect("mnemonic consumes old pending release", !BlockMenu(policy, 0xF100, 0, windowA, 132));
    policy = {}; Tap(policy);
    Expect("other system commands remain enabled", !BlockMenu(policy, 0xF060, 0, windowA, 131));
    policy = {}; Tap(policy);
    Expect("low command bits do not bypass protection", BlockMenu(policy, 0xF103, 0, windowA, 131));

    policy = {}; Tap(policy);
    Expect("another foreground window is never blocked", !BlockMenu(policy, 0xF100, 0, windowB, 131));
    policy = {}; Key(policy, true, false, 100); Key(policy, true, true, 130, windowB);
    Expect("changing windows during the press cancels protection", !BlockMenu(policy, 0xF100, 0, windowB, 131));
    policy = {}; Tap(policy);
    Expect("stale release cannot block a later menu", !BlockMenu(policy, 0xF100, 0, windowA, 1131));
    policy = {}; Tap(policy);
    Expect("no foreground window means no suppression", !BlockMenu(policy, 0xF100, 0, 0, 131));
    policy = {}; Key(policy, true, true, 130);
    Expect("starting while Alt is held never guesses a press", !BlockMenu(policy, 0xF100, 0, windowA, 131));

    policy = {}; Key(policy, true, false, 100); Key(policy, true, false, 110, windowA, true); Key(policy, true, true, 130);
    Expect("holding right Alt with repeat still protects release", BlockMenu(policy, 0xF100, 0, windowA, 131));
    policy = {}; Key(policy, true, false, 100); Key(policy, false, false, 105); Key(policy, true, false, 110, windowA, true); Key(policy, true, true, 130);
    Expect("repeat cannot rearm an Alt combination", !BlockMenu(policy, 0xF100, 0, windowA, 131));
    policy = {}; Key(policy, true, false, 100, windowA, true); Key(policy, true, true, 130);
    Expect("repeat without initial down does not count as a tap", !BlockMenu(policy, 0xF100, 0, windowA, 131));

    policy = {}; Tap(policy); Key(policy, false, false, 131);
    Expect("a following F10 or other key clears pending suppression", !BlockMenu(policy, 0xF100, 0, windowA, 132));
    policy = {}; Tap(policy, 0xFFFFFFE0u);
    Expect("uptime wraparound does not bypass protection", BlockMenu(policy, 0xF100, 0, windowA, 5));
    policy = {}; Tap(policy); Tap(policy, 200);
    Expect("a second right Alt tap protects its own release", BlockMenu(policy, 0xF100, 0, windowA, 231));

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
