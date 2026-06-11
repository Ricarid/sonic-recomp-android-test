// =============================================================================
// apu_android.cpp — Driver de áudio Android para a engine XAudio
//
// Equivale a apu/driver/sdl2_driver.cpp do PC.
//
// O PC usa SDL2 para abrir um device de áudio e chama um callback PPC
// (código recompilado do Xbox 360) periodicamente para preencher buffers.
//
// Aqui fazemos o mesmo via AAudio:
//   1. AAudio chama AaudioDataCallback() em thread de tempo real
//   2. O callback chama XAudioSubmitFrame() que a engine fornece
//   3. XAudioSubmitFrame() converte big-endian → little-endian e escreve no buffer
//
// Implementa as funções globais declaradas em apu/audio.h:
//   XAudioInitializeSystem()
//   XAudioRegisterClient()
//   XAudioSubmitFrame()
//   XAudioRegisterRenderDriverClient()
//   XAudioUnregisterRenderDriverClient()
//   XAudioSubmitRenderDriverFrame()
// =============================================================================

#include "apu_android.h"
#include "../os/android/logger_android.h"
#include "../audio/audio_android.h"

#include <aaudio/AAudio.h>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <bit>

// ---------------------------------------------------------------------------
// Parâmetros idênticos ao PC (apu/audio.h)
// ---------------------------------------------------------------------------

static constexpr int32_t  XAUDIO_SAMPLES_HZ   = 48000;
static constexpr int32_t  XAUDIO_NUM_CHANNELS  = 6;   // 5.1 surround (downmix para estéreo)
static constexpr int32_t  XAUDIO_NUM_SAMPLES   = 256;

// Buffer de amostras: engine preenche em formato big-endian float32 × 6 canais
static float g_xaudioBuffer[XAUDIO_NUM_SAMPLES * XAUDIO_NUM_CHANNELS] = {};
static float g_stereoBuffer[XAUDIO_NUM_SAMPLES * 2]                   = {};  // downmix estéreo para AAudio

// Callback registrado pela engine (ponteiro de função PPC recompilado)
// No Android não temos PPCFunc*, guardamos um ponteiro genérico + param
using AudioCallbackFn = void (*)(void* param);
static AudioCallbackFn  g_clientCallback      = nullptr;
static void*            g_clientCallbackParam = nullptr;

// ---------------------------------------------------------------------------
// Downmix 5.1 → estéreo
// O Xbox 360 gera 6 canais em layout: FL FR FC LFE SL SR
// Misturamos para L/R da forma mais simples
// ---------------------------------------------------------------------------

static void Downmix51ToStereo(const float* src6ch, float* dst2ch, int32_t frames)
{
    for (int32_t i = 0; i < frames; ++i)
    {
        const float* s = src6ch + i * 6;
        // FL FR FC(centre shared) SL SR
        float l = s[0] + s[2] * 0.707f + s[4] * 0.5f;
        float r = s[1] + s[2] * 0.707f + s[5] * 0.5f;
        // Clamp simples (sem saturar)
        dst2ch[i * 2    ] = l > 1.f ? 1.f : (l < -1.f ? -1.f : l);
        dst2ch[i * 2 + 1] = r > 1.f ? 1.f : (r < -1.f ? -1.f : r);
    }
}

// ---------------------------------------------------------------------------
// ByteSwap float (Xbox 360 é big-endian)
// ---------------------------------------------------------------------------

static inline float ByteSwapFloat(float f)
{
    uint32_t v;
    memcpy(&v, &f, 4);
    v = __builtin_bswap32(v);
    memcpy(&f, &v, 4);
    return f;
}

// ---------------------------------------------------------------------------
// XAudioInitializeSystem — chamada no KiSystemStartup do PC
// No Android: AAudio já foi inicializado pelo Audio_Init() no JNI.
// Esta função apenas loga.
// ---------------------------------------------------------------------------

void XAudioInitializeSystem()
{
    LOGI("XAudioInitializeSystem: usando AAudio backend");
}

// ---------------------------------------------------------------------------
// XAudioRegisterClient — engine registra seu callback de preenchimento
// ---------------------------------------------------------------------------

void XAudioRegisterClient(void* callback, void* param)
{
    g_clientCallback      = reinterpret_cast<AudioCallbackFn>(callback);
    g_clientCallbackParam = param;
    LOGI("XAudioRegisterClient: callback=%p param=%p", callback, param);
}

// ---------------------------------------------------------------------------
// XAudioSubmitFrame — engine entrega um frame de 256 amostras × 6 canais
//
// Formato do Xbox 360: float32 big-endian, layout intercalado por canal:
//   canal0[0..255], canal1[0..255], ..., canal5[0..255]
//
// Precisamos:
//   1. Converter BE → LE
//   2. De-interleave: coluna por canal → linha por frame
//   3. Downmix 5.1 → estéreo
//   4. Enviar para Audio_SubmitFrames() (AAudio)
// ---------------------------------------------------------------------------

void XAudioSubmitFrame(void* samples)
{
    if (!samples) return;

    const uint32_t* src = static_cast<const uint32_t*>(samples);

    // Passo 1 + 2: BE float → LE, e de-interleave canal→frame
    for (int32_t frame = 0; frame < XAUDIO_NUM_SAMPLES; ++frame)
    {
        for (int32_t ch = 0; ch < XAUDIO_NUM_CHANNELS; ++ch)
        {
            uint32_t rawBE = src[ch * XAUDIO_NUM_SAMPLES + frame];
            uint32_t rawLE = __builtin_bswap32(rawBE);
            float    f;
            memcpy(&f, &rawLE, 4);
            g_xaudioBuffer[frame * XAUDIO_NUM_CHANNELS + ch] = f;
        }
    }

    // Passo 3: downmix
    Downmix51ToStereo(g_xaudioBuffer, g_stereoBuffer, XAUDIO_NUM_SAMPLES);

    // Passo 4: entrega ao backend de áudio Android
    Audio_SubmitFrames(g_stereoBuffer, XAUDIO_NUM_SAMPLES);
}

// ---------------------------------------------------------------------------
// XAudioRegisterRenderDriverClient / Unregister / SubmitRenderDriverFrame
// Idêntico ao audio.cpp do PC; apenas chama as funções acima
// ---------------------------------------------------------------------------

static constexpr uint32_t AUDIO_DRIVER_KEY = (uint32_t)('DAUD');

uint32_t XAudioRegisterRenderDriverClient(void* callbackPtr, void* driverOut)
{
    *static_cast<uint32_t*>(driverOut) = AUDIO_DRIVER_KEY;
    XAudioRegisterClient(callbackPtr, nullptr);
    return 0;
}

uint32_t XAudioUnregisterRenderDriverClient(uint32_t /*driver*/)
{
    g_clientCallback      = nullptr;
    g_clientCallbackParam = nullptr;
    return 0;
}

uint32_t XAudioSubmitRenderDriverFrame(uint32_t /*driver*/, void* samples)
{
    XAudioSubmitFrame(samples);
    return 0;
}

// ---------------------------------------------------------------------------
// Pump: chamado pelo loop principal para acionar o callback da engine
// (no PC isso roda em thread separada; no Android pode rodar no game loop)
// ---------------------------------------------------------------------------

void XAudio_Pump()
{
    if (g_clientCallback)
        g_clientCallback(g_clientCallbackParam);
}
