#pragma once
#include <cstdint>
#include <mutex>

// ---------------------------------------------------------------------------
// Bitmask de botões — compatível com layout Xbox 360
// ---------------------------------------------------------------------------

enum VpadButton : uint32_t
{
    VPAD_A      = 1 << 0,
    VPAD_B      = 1 << 1,
    VPAD_X      = 1 << 2,
    VPAD_Y      = 1 << 3,
    VPAD_LB     = 1 << 4,
    VPAD_RB     = 1 << 5,
    VPAD_LT     = 1 << 6,
    VPAD_RT     = 1 << 7,
    VPAD_START  = 1 << 8,
    VPAD_SELECT = 1 << 9,
    VPAD_DPAD_U = 1 << 10,
    VPAD_DPAD_D = 1 << 11,
    VPAD_DPAD_L = 1 << 12,
    VPAD_DPAD_R = 1 << 13,
};

struct VirtualGamepadState
{
    uint32_t buttons     = 0;
    float    leftStickX  = 0.f; // [-1, +1]
    float    leftStickY  = 0.f; // [-1, +1]
    float    rightStickX = 0.f;
    float    rightStickY = 0.f;
};

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

void                VirtualGamepad_Update(float screenW, float screenH);
VirtualGamepadState VirtualGamepad_GetState();
bool                VirtualGamepad_IsPressed(uint32_t button);
void                VirtualGamepad_Reset();
