#pragma once
#include <stdint.h>

struct MenuPolicy {
    bool down = false;
    bool candidate = false;
    bool pending = false;
    uintptr_t window = 0;
    uint32_t released = 0;
};

inline void ObserveKey(MenuPolicy& policy, bool rightAlt, bool up, bool repeat,
                       bool otherModifiers, uintptr_t window, uint32_t time) {
    if (!rightAlt) {
        policy.candidate = false;
        policy.pending = false;
        return;
    }
    if (!up) {
        if (!repeat) {
            policy.down = true;
            policy.candidate = !otherModifiers && window != 0;
            policy.pending = false;
            policy.window = window;
        }
        return;
    }
    policy.pending = policy.down && policy.candidate && !otherModifiers &&
                     window != 0 && window == policy.window;
    policy.released = time;
    policy.down = false;
    policy.candidate = false;
}

inline bool BlockMenu(MenuPolicy& policy, uintptr_t command, intptr_t character,
                      uintptr_t foregroundWindow, uint32_t time) {
    if ((command & 0xFFF0) != 0xF100) return false;
    bool block = policy.pending && character == 0 && foregroundWindow != 0 &&
                 foregroundWindow == policy.window &&
                 static_cast<uint32_t>(time - policy.released) <= 1000;
    policy.pending = false;
    return block;
}
