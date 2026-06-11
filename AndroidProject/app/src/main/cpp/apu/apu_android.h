#pragma once

// =============================================================================
// apu_android.h — Driver de áudio Android (equivalente ao sdl2_driver.cpp do PC)
// =============================================================================

#include <cstdint>

// Inicializa o subsistema de áudio (chamada no startup)
void XAudioInitializeSystem();

// Registra o callback de preenchimento de amostras da engine
void XAudioRegisterClient(void* callback, void* param);

// Entrega um frame de amostras do Xbox 360 (big-endian 5.1) ao backend Android
void XAudioSubmitFrame(void* samples);

// API de render driver (chamada pela engine via imports.cpp)
uint32_t XAudioRegisterRenderDriverClient(void* callbackPtr, void* driverOut);
uint32_t XAudioUnregisterRenderDriverClient(uint32_t driver);
uint32_t XAudioSubmitRenderDriverFrame(uint32_t driver, void* samples);

// Pump — deve ser chamado no game loop para acionar o callback da engine
void XAudio_Pump();
