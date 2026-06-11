#include "touch_input.h"
#include "../os/android/logger_android.h"

#include <android/input.h>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// Estado interno de touches
// ---------------------------------------------------------------------------

static TouchState g_touches[MAX_TOUCH_POINTERS] = {};
static int32_t    g_touchCount = 0;
static std::mutex g_touchMutex;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int32_t FindSlotByPointerId(int32_t pointerId)
{
    for (int32_t i = 0; i < MAX_TOUCH_POINTERS; ++i)
        if (g_touches[i].active && g_touches[i].pointerId == pointerId)
            return i;
    return -1;
}

static int32_t AllocSlot(int32_t pointerId)
{
    for (int32_t i = 0; i < MAX_TOUCH_POINTERS; ++i)
    {
        if (!g_touches[i].active)
        {
            g_touches[i].pointerId = pointerId;
            g_touches[i].active    = true;
            return i;
        }
    }
    return -1; // sem slot livre
}

// ---------------------------------------------------------------------------
// Processamento do AInputEvent de toque
// ---------------------------------------------------------------------------

int32_t Touch_HandleEvent(AInputEvent* event)
{
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION)
        return 0;

    const int32_t action    = AMotionEvent_getAction(event);
    const int32_t actionCode= action & AMOTION_EVENT_ACTION_MASK;
    const int32_t pointerIdx= (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                              >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    std::lock_guard<std::mutex> lock(g_touchMutex);

    switch (actionCode)
    {
        // ------------------------------------------------------------------
        // Primeiro dedo toca a tela
        // ------------------------------------------------------------------
        case AMOTION_EVENT_ACTION_DOWN:
        {
            int32_t pid  = AMotionEvent_getPointerId(event, 0);
            int32_t slot = AllocSlot(pid);
            if (slot >= 0)
            {
                g_touches[slot].x = AMotionEvent_getX(event, 0);
                g_touches[slot].y = AMotionEvent_getY(event, 0);
                g_touches[slot].pressure = AMotionEvent_getPressure(event, 0);
                LOGD("Touch DOWN slot=%d pid=%d x=%.1f y=%.1f",
                     slot, pid, g_touches[slot].x, g_touches[slot].y);
            }
            g_touchCount = 1;
            break;
        }

        // ------------------------------------------------------------------
        // Dedo adicional toca a tela (multitouch)
        // ------------------------------------------------------------------
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        {
            int32_t pid  = AMotionEvent_getPointerId(event, pointerIdx);
            int32_t slot = AllocSlot(pid);
            if (slot >= 0)
            {
                g_touches[slot].x = AMotionEvent_getX(event, pointerIdx);
                g_touches[slot].y = AMotionEvent_getY(event, pointerIdx);
                g_touches[slot].pressure = AMotionEvent_getPressure(event, pointerIdx);
                LOGD("Touch POINTER_DOWN slot=%d pid=%d x=%.1f y=%.1f",
                     slot, pid, g_touches[slot].x, g_touches[slot].y);
            }
            g_touchCount = (int32_t)AMotionEvent_getPointerCount(event);
            break;
        }

        // ------------------------------------------------------------------
        // Movimento de um ou mais dedos
        // ------------------------------------------------------------------
        case AMOTION_EVENT_ACTION_MOVE:
        {
            int32_t count = (int32_t)AMotionEvent_getPointerCount(event);
            for (int32_t i = 0; i < count; ++i)
            {
                int32_t pid  = AMotionEvent_getPointerId(event, i);
                int32_t slot = FindSlotByPointerId(pid);
                if (slot >= 0)
                {
                    g_touches[slot].prevX = g_touches[slot].x;
                    g_touches[slot].prevY = g_touches[slot].y;
                    g_touches[slot].x = AMotionEvent_getX(event, i);
                    g_touches[slot].y = AMotionEvent_getY(event, i);
                    g_touches[slot].pressure = AMotionEvent_getPressure(event, i);
                }
            }
            g_touchCount = count;
            break;
        }

        // ------------------------------------------------------------------
        // Último dedo sai da tela
        // ------------------------------------------------------------------
        case AMOTION_EVENT_ACTION_UP:
        {
            int32_t pid  = AMotionEvent_getPointerId(event, 0);
            int32_t slot = FindSlotByPointerId(pid);
            if (slot >= 0)
            {
                LOGD("Touch UP slot=%d pid=%d", slot, pid);
                memset(&g_touches[slot], 0, sizeof(TouchState));
            }
            g_touchCount = 0;
            break;
        }

        // ------------------------------------------------------------------
        // Um dedo intermediário sai (multitouch)
        // ------------------------------------------------------------------
        case AMOTION_EVENT_ACTION_POINTER_UP:
        {
            int32_t pid  = AMotionEvent_getPointerId(event, pointerIdx);
            int32_t slot = FindSlotByPointerId(pid);
            if (slot >= 0)
            {
                LOGD("Touch POINTER_UP slot=%d pid=%d", slot, pid);
                memset(&g_touches[slot], 0, sizeof(TouchState));
            }
            g_touchCount = std::max(0,
                (int32_t)AMotionEvent_getPointerCount(event) - 1);
            break;
        }

        // ------------------------------------------------------------------
        // Gesto cancelado (ligação, notificação, etc.)
        // ------------------------------------------------------------------
        case AMOTION_EVENT_ACTION_CANCEL:
        {
            LOGD("Touch CANCEL — limpando todos os slots");
            memset(g_touches, 0, sizeof(g_touches));
            g_touchCount = 0;
            break;
        }

        default:
            return 0;
    }

    return 1; // evento consumido
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int32_t Touch_GetCount()
{
    std::lock_guard<std::mutex> lock(g_touchMutex);
    return g_touchCount;
}

bool Touch_GetState(int32_t slot, TouchState& out)
{
    if (slot < 0 || slot >= MAX_TOUCH_POINTERS) return false;
    std::lock_guard<std::mutex> lock(g_touchMutex);
    if (!g_touches[slot].active) return false;
    out = g_touches[slot];
    return true;
}

void Touch_Reset()
{
    std::lock_guard<std::mutex> lock(g_touchMutex);
    memset(g_touches, 0, sizeof(g_touches));
    g_touchCount = 0;
}
