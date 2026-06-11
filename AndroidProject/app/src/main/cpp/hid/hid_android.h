#pragma once

// =============================================================================
// hid_android.h — API pública do driver de entrada Android
// =============================================================================

#include <cstdint>

// Processamento de eventos de entrada (chamados pelo loop de eventos Android)
void HID_OnKeyEvent(int32_t action, int32_t keyCode);
void HID_OnMotionEvent(float leftX, float leftY,
                       float rightX, float rightY,
                       float lt, float rt);
void HID_OnGamepadConnected(bool connected);

// API compatível com hid::GetState/SetState do PC
uint32_t HID_GetState(uint32_t dwUserIndex, void* pState);
uint32_t HID_SetState(uint32_t dwUserIndex, const void* pVibration);

// Accessors de vibração (lidos pelo JNI para enviar ao Java)
uint16_t HID_GetVibrationLeft();
uint16_t HID_GetVibrationRight();

bool HID_IsGamepadConnected();
