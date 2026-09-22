#pragma once

#include "Arduino.h"

uint8_t mock_atomic_enter();
void mock_atomic_exit(uint8_t saved);

struct MockAtomicRestore {
    uint8_t saved = mock_atomic_enter();
    bool active = true;
    ~MockAtomicRestore() noexcept(false) { mock_atomic_exit(saved); }
};

#define ATOMIC_RESTORESTATE MockAtomicRestore
#define ATOMIC_BLOCK(type) for (type atomic_state; atomic_state.active; atomic_state.active = false)