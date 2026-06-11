#include "virtual_gamepad.h"
#include "touch_input.h"
#include "../os/android/logger_android.h"

#include <cmath>
#include <algorithm>
#include <cstring>

// ---------------------------------------------------------------------------
// Layout do gamepad virtual
// Coordenadas em espaço normalizado [0,1] para adaptar qualquer aspect ratio
// ---------------------------------------------------------------------------

// Joystick esquerdo — canto inferior esquerdo
static constexpr float STICK_L_CENTER_X = 0.15f;
static constexpr float STICK_L_CENTER_Y = 0.75f;
static constexpr float STICK_RADIUS     = 0.10f;

// Botões ação — canto inferior direito (A B X Y)
static constexpr float BTN_A_X = 0.88f; static constexpr float BTN_A_Y = 0.78f;
static constexpr float BTN_B_X = 0.93f; static constexpr float BTN_B_Y = 0.70f;
static constexpr float BTN_X_X = 0.83f; static constexpr float BTN_X_Y = 0.70f;
static constexpr float BTN_Y_X = 0.88f; static constexpr float BTN_Y_Y = 0.62f;

// Gatilhos e bumpers — topo
static constexpr float BTN_LB_X = 0.08f; static constexpr float BTN_LB_Y = 0.08f;
static constexpr float BTN_RB_X = 0.92f; static constexpr float BTN_RB_Y = 0.08f;
static constexpr float BTN_LT_X = 0.08f; static constexpr float BTN_LT_Y = 0.18f;
static constexpr float BTN_RT_X = 0.92f; static constexpr float BTN_RT_Y = 0.18f;

// Start / Select — centro topo
static constexpr float BTN_START_X = 0.58f; static constexpr float BTN_START_Y = 0.08f;
static constexpr float BTN_SEL_X   = 0.42f; static constexpr float BTN_SEL_Y   = 0.08f;

// Raio de toque para botões
static constexpr float BTN_HIT_RADIUS = 0.065f;

// ---------------------------------------------------------------------------
// Estado do gamepad
// ---------------------------------------------------------------------------

static VirtualGamepadState g_state = {};
static std::mutex g_padMutex;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float Dist2D(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by;
    return sqrtf(dx * dx + dy * dy);
}

static void NormTouchPos(float rawX, float rawY,
                         float screenW, float screenH,
                         float& nx, float& ny)
{
    nx = rawX / screenW;
    ny = rawY / screenH;
}

// ---------------------------------------------------------------------------
// Atualiza o estado do pad com base nos touches ativos
// ---------------------------------------------------------------------------

void VirtualGamepad_Update(float screenW, float screenH)
{
    // Reset de eixos; botões mantém estado até liberados
    std::lock_guard<std::mutex> lock(g_padMutex);

    uint32_t newButtons = 0;
    float    stickLX    = 0.f, stickLY = 0.f;

    int32_t count = Touch_GetCount();

    for (int32_t slot = 0; slot < MAX_TOUCH_POINTERS; ++slot)
    {
        TouchState ts;
        if (!Touch_GetState(slot, ts)) continue;

        float nx, ny;
        NormTouchPos(ts.x, ts.y, screenW, screenH, nx, ny);

        // -----------------------------------------------------------------
        // Joystick esquerdo
        // -----------------------------------------------------------------
        float dStick = Dist2D(nx, ny, STICK_L_CENTER_X, STICK_L_CENTER_Y);
        if (dStick < STICK_RADIUS * 1.5f)
        {
            // Converte posição do toque em deslocamento normalizado [-1, +1]
            stickLX = std::clamp((nx - STICK_L_CENTER_X) / STICK_RADIUS, -1.f, 1.f);
            stickLY = std::clamp((ny - STICK_L_CENTER_Y) / STICK_RADIUS, -1.f, 1.f);
            continue;
        }

        // -----------------------------------------------------------------
        // Botões de ação
        // -----------------------------------------------------------------
        if (Dist2D(nx, ny, BTN_A_X, BTN_A_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_A;
        if (Dist2D(nx, ny, BTN_B_X, BTN_B_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_B;
        if (Dist2D(nx, ny, BTN_X_X, BTN_X_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_X;
        if (Dist2D(nx, ny, BTN_Y_X, BTN_Y_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_Y;

        // Bumpers e gatilhos
        if (Dist2D(nx, ny, BTN_LB_X, BTN_LB_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_LB;
        if (Dist2D(nx, ny, BTN_RB_X, BTN_RB_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_RB;
        if (Dist2D(nx, ny, BTN_LT_X, BTN_LT_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_LT;
        if (Dist2D(nx, ny, BTN_RT_X, BTN_RT_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_RT;

        // Start / Select
        if (Dist2D(nx, ny, BTN_START_X, BTN_START_Y) < BTN_HIT_RADIUS) newButtons |= VPAD_START;
        if (Dist2D(nx, ny, BTN_SEL_X,   BTN_SEL_Y)   < BTN_HIT_RADIUS) newButtons |= VPAD_SELECT;
    }

    (void)count; // usado indiretamente via Touch_GetState

    g_state.buttons      = newButtons;
    g_state.leftStickX   = stickLX;
    g_state.leftStickY   = stickLY;
    // Joystick direito reservado para futura câmera touch
    g_state.rightStickX  = 0.f;
    g_state.rightStickY  = 0.f;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

VirtualGamepadState VirtualGamepad_GetState()
{
    std::lock_guard<std::mutex> lock(g_padMutex);
    return g_state;
}

bool VirtualGamepad_IsPressed(uint32_t button)
{
    std::lock_guard<std::mutex> lock(g_padMutex);
    return (g_state.buttons & button) != 0;
}

void VirtualGamepad_Reset()
{
    std::lock_guard<std::mutex> lock(g_padMutex);
    memset(&g_state, 0, sizeof(g_state));
}
