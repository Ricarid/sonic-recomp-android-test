#pragma once
#include <android/input.h>
#include <mutex>
#include <cstdint>

// ---------------------------------------------------------------------------
// Multitouch — máximo de ponteiros simultâneos rastreados
// ---------------------------------------------------------------------------

static constexpr int MAX_TOUCH_POINTERS = 10;

struct TouchState
{
    bool    active     = false;
    int32_t pointerId  = -1;
    float   x          = 0.f;
    float   y          = 0.f;
    float   prevX      = 0.f;
    float   prevY      = 0.f;
    float   pressure   = 0.f;
};

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

int32_t Touch_HandleEvent(AInputEvent* event);
int32_t Touch_GetCount();
bool    Touch_GetState(int32_t slot, TouchState& out);
void    Touch_Reset();
