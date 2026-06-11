// =============================================================================
// hid_android.cpp — Driver de entrada para Android
//
// Equivale a hid/driver/sdl_hid.cpp do PC.
//
// Suporta:
//   1. Controles físicos via Android GameController API (Bluetooth/USB)
//      → mapeados para XAMINPUT_GAMEPAD igual ao PC
//   2. Touch virtual (delegado ao touch_input/virtual_gamepad existentes)
//   3. Vibração via Vibrator (chamada Java)
//
// A engine consulta hid::GetState()/SetState() exatamente como no PC;
// esta implementação preenche os mesmos structs XAMINPUT_*.
// =============================================================================

#include "hid_android.h"
#include "../os/android/logger_android.h"
#include "../input/virtual_gamepad.h"

#include <android/input.h>
#include <mutex>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Tipos XAM (replicados do PC para compilar sem o kernel completo)
// ---------------------------------------------------------------------------

// Botões Xbox / XAMINPUT compatíveis
#define XINPUT_GAMEPAD_DPAD_UP        0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN      0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT      0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT     0x0008
#define XINPUT_GAMEPAD_START          0x0010
#define XINPUT_GAMEPAD_BACK           0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB     0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB    0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER  0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A              0x1000
#define XINPUT_GAMEPAD_B              0x2000
#define XINPUT_GAMEPAD_X              0x4000
#define XINPUT_GAMEPAD_Y              0x8000

// ---------------------------------------------------------------------------
// Estado do gamepad (thread-safe)
// ---------------------------------------------------------------------------

struct AndroidGamepadState
{
    uint16_t buttons    = 0;
    uint8_t  leftTrigger  = 0;
    uint8_t  rightTrigger = 0;
    int16_t  leftStickX   = 0;
    int16_t  leftStickY   = 0;
    int16_t  rightStickX  = 0;
    int16_t  rightStickY  = 0;
    bool     connected    = false;
};

static AndroidGamepadState g_gamepad;
static std::mutex          g_gamepadMutex;

// Vibração pendente (enviada ao Java)
static uint16_t g_vibrationLeft  = 0;
static uint16_t g_vibrationRight = 0;

// ---------------------------------------------------------------------------
// Conversão de eixo analógico Android → int16_t normalizado
// Android usa float [-1.0, 1.0]; XInput usa int16_t [-32768, 32767]
// ---------------------------------------------------------------------------

static int16_t AxisToInt16(float value)
{
    int32_t v = static_cast<int32_t>(value * 32767.0f);
    if (v >  32767) v =  32767;
    if (v < -32768) v = -32768;
    return static_cast<int16_t>(v);
}

// Zona morta: ignora movimentos muito pequenos
static constexpr float DEADZONE = 0.12f;
static float ApplyDeadzone(float v)
{
    return (std::fabs(v) < DEADZONE) ? 0.0f : v;
}

// ---------------------------------------------------------------------------
// Processamento de eventos Android (chamado pelo loop nativo)
// ---------------------------------------------------------------------------

void HID_OnKeyEvent(int32_t action, int32_t keyCode)
{
    std::lock_guard<std::mutex> lock(g_gamepadMutex);
    bool pressed = (action == AKEY_EVENT_ACTION_DOWN);

    // Mapeia teclas de controle físico para botões XAMINPUT
    switch (keyCode)
    {
        case AKEYCODE_BUTTON_A:      pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_A      : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_A);      break;
        case AKEYCODE_BUTTON_B:      pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_B      : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_B);      break;
        case AKEYCODE_BUTTON_X:      pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_X      : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_X);      break;
        case AKEYCODE_BUTTON_Y:      pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_Y      : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_Y);      break;
        case AKEYCODE_BUTTON_L1:     pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER  : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_LEFT_SHOULDER);  break;
        case AKEYCODE_BUTTON_R1:     pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_RIGHT_SHOULDER); break;
        case AKEYCODE_BUTTON_THUMBL: pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_LEFT_THUMB     : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_LEFT_THUMB);     break;
        case AKEYCODE_BUTTON_THUMBR: pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_RIGHT_THUMB    : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_RIGHT_THUMB);    break;
        case AKEYCODE_BUTTON_START:  pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_START  : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_START);  break;
        case AKEYCODE_BUTTON_SELECT: pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_BACK   : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_BACK);   break;
        case AKEYCODE_DPAD_UP:       pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_DPAD_UP    : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_DPAD_UP);    break;
        case AKEYCODE_DPAD_DOWN:     pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_DPAD_DOWN  : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_DPAD_DOWN);  break;
        case AKEYCODE_DPAD_LEFT:     pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_DPAD_LEFT  : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_DPAD_LEFT);  break;
        case AKEYCODE_DPAD_RIGHT:    pressed ? g_gamepad.buttons |= XINPUT_GAMEPAD_DPAD_RIGHT : (g_gamepad.buttons &= ~XINPUT_GAMEPAD_DPAD_RIGHT); break;
        default: break;
    }
    g_gamepad.connected = true;
}

void HID_OnMotionEvent(float leftX, float leftY,
                       float rightX, float rightY,
                       float lt, float rt)
{
    std::lock_guard<std::mutex> lock(g_gamepadMutex);

    g_gamepad.leftStickX  = AxisToInt16(ApplyDeadzone(leftX));
    g_gamepad.leftStickY  = AxisToInt16(-ApplyDeadzone(leftY)); // Y invertido
    g_gamepad.rightStickX = AxisToInt16(ApplyDeadzone(rightX));
    g_gamepad.rightStickY = AxisToInt16(-ApplyDeadzone(rightY));

    // Triggers: Android dá [0,1], XInput quer [0,255]
    g_gamepad.leftTrigger  = static_cast<uint8_t>(std::min(lt * 255.0f, 255.0f));
    g_gamepad.rightTrigger = static_cast<uint8_t>(std::min(rt * 255.0f, 255.0f));
    g_gamepad.connected    = true;
}

void HID_OnGamepadConnected(bool connected)
{
    std::lock_guard<std::mutex> lock(g_gamepadMutex);
    g_gamepad.connected = connected;
    if (!connected)
        memset(&g_gamepad, 0, sizeof(g_gamepad));
    LOGI("Gamepad %s", connected ? "conectado" : "desconectado");
}

// ---------------------------------------------------------------------------
// API pública compatível com hid::GetState / hid::SetState do PC
// ---------------------------------------------------------------------------

// Preenche XAMINPUT_STATE com o estado atual do controle/virtual gamepad
uint32_t HID_GetState(uint32_t /*dwUserIndex*/, void* pStateOut)
{
    if (!pStateOut) return 1; // ERROR

    // Struct simplificada (mesmo layout do XAMINPUT_STATE do PC)
    struct XAMInputState
    {
        uint32_t dwPacketNumber;
        struct
        {
            uint16_t wButtons;
            uint8_t  bLeftTrigger;
            uint8_t  bRightTrigger;
            int16_t  sThumbLX;
            int16_t  sThumbLY;
            int16_t  sThumbRX;
            int16_t  sThumbRY;
        } Gamepad;
    };

    auto* state = static_cast<XAMInputState*>(pStateOut);
    memset(state, 0, sizeof(*state));

    std::lock_guard<std::mutex> lock(g_gamepadMutex);

    if (!g_gamepad.connected)
    {
        // Tenta pegar estado do virtual gamepad (touch controls)
        // VirtualGamepadState usa float [-1,1] e uint32_t buttons
        VirtualGamepadState vgs = VirtualGamepad_GetState();

        // Mapeia bitmask VPAD_* → XINPUT_GAMEPAD_*
        uint16_t btns = 0;
        if (vgs.buttons & VPAD_A)      btns |= XINPUT_GAMEPAD_A;
        if (vgs.buttons & VPAD_B)      btns |= XINPUT_GAMEPAD_B;
        if (vgs.buttons & VPAD_X)      btns |= XINPUT_GAMEPAD_X;
        if (vgs.buttons & VPAD_Y)      btns |= XINPUT_GAMEPAD_Y;
        if (vgs.buttons & VPAD_LB)     btns |= XINPUT_GAMEPAD_LEFT_SHOULDER;
        if (vgs.buttons & VPAD_RB)     btns |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (vgs.buttons & VPAD_START)  btns |= XINPUT_GAMEPAD_START;
        if (vgs.buttons & VPAD_SELECT) btns |= XINPUT_GAMEPAD_BACK;
        if (vgs.buttons & VPAD_DPAD_U) btns |= XINPUT_GAMEPAD_DPAD_UP;
        if (vgs.buttons & VPAD_DPAD_D) btns |= XINPUT_GAMEPAD_DPAD_DOWN;
        if (vgs.buttons & VPAD_DPAD_L) btns |= XINPUT_GAMEPAD_DPAD_LEFT;
        if (vgs.buttons & VPAD_DPAD_R) btns |= XINPUT_GAMEPAD_DPAD_RIGHT;

        // LT/RT do virtual gamepad usam VPAD_LT/RT como botão digital
        uint8_t lt = (vgs.buttons & VPAD_LT) ? 255 : 0;
        uint8_t rt = (vgs.buttons & VPAD_RT) ? 255 : 0;

        state->Gamepad.wButtons      = btns;
        state->Gamepad.bLeftTrigger  = lt;
        state->Gamepad.bRightTrigger = rt;
        // VirtualGamepadState usa float [-1,1]; converter para int16_t
        state->Gamepad.sThumbLX      = AxisToInt16(vgs.leftStickX);
        state->Gamepad.sThumbLY      = AxisToInt16(vgs.leftStickY);
        state->Gamepad.sThumbRX      = AxisToInt16(vgs.rightStickX);
        state->Gamepad.sThumbRY      = AxisToInt16(vgs.rightStickY);
    }
    else
    {
        state->Gamepad.wButtons      = g_gamepad.buttons;
        state->Gamepad.bLeftTrigger  = g_gamepad.leftTrigger;
        state->Gamepad.bRightTrigger = g_gamepad.rightTrigger;
        state->Gamepad.sThumbLX      = g_gamepad.leftStickX;
        state->Gamepad.sThumbLY      = g_gamepad.leftStickY;
        state->Gamepad.sThumbRX      = g_gamepad.rightStickX;
        state->Gamepad.sThumbRY      = g_gamepad.rightStickY;
    }

    static uint32_t packetNum = 0;
    state->dwPacketNumber = ++packetNum;
    return 0; // ERROR_SUCCESS
}

// Recebe pedido de vibração da engine
uint32_t HID_SetState(uint32_t /*dwUserIndex*/, const void* pVibration)
{
    if (!pVibration) return 1;

    struct XAMVibration { uint16_t wLeftMotorSpeed; uint16_t wRightMotorSpeed; };
    const auto* vib = static_cast<const XAMVibration*>(pVibration);

    g_vibrationLeft  = vib->wLeftMotorSpeed;
    g_vibrationRight = vib->wRightMotorSpeed;

    // Vibração é enviada para o Java via JNI (ver jni_bridge.cpp)
    return 0;
}

uint16_t HID_GetVibrationLeft()  { return g_vibrationLeft;  }
uint16_t HID_GetVibrationRight() { return g_vibrationRight; }

bool HID_IsGamepadConnected() { return g_gamepad.connected; }
