#include "audio_android.h"
#include "../os/android/logger_android.h"

#include <aaudio/AAudio.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <atomic>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Parâmetros de stream
// ---------------------------------------------------------------------------

static constexpr int32_t  SAMPLE_RATE     = 48000;
static constexpr int32_t  CHANNEL_COUNT   = 2;     // estéreo
static constexpr int32_t  FRAMES_PER_CB   = 256;   // baixa latência

// ---------------------------------------------------------------------------
// Estado AAudio
// ---------------------------------------------------------------------------

static AAudioStream*      g_aaStream      = nullptr;
static std::atomic<bool>  g_audioRunning  {false};
static std::atomic<bool>  g_audioPaused   {false};

// Buffer circular simples para o callback de áudio
static float              g_mixBuffer[FRAMES_PER_CB * CHANNEL_COUNT * 4] = {};
static std::atomic<int>   g_writePos{0}, g_readPos{0};

// ---------------------------------------------------------------------------
// Callback AAudio — roda em thread de tempo real (NÃO aloca memória aqui)
// ---------------------------------------------------------------------------

static aaudio_data_callback_result_t AaudioDataCallback(
    AAudioStream* /*stream*/,
    void*         /*userData*/,
    void*          audioData,
    int32_t        numFrames)
{
    float* out     = static_cast<float*>(audioData);
    int32_t samples = numFrames * CHANNEL_COUNT;

    if (g_audioPaused.load())
    {
        memset(out, 0, samples * sizeof(float));
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    // Copia do buffer circular; silêncio se underrun
    int rp = g_readPos.load();
    for (int32_t i = 0; i < samples; ++i)
    {
        out[i] = g_mixBuffer[rp];
        g_mixBuffer[rp] = 0.f; // limpa após leitura
        rp = (rp + 1) % (FRAMES_PER_CB * CHANNEL_COUNT * 4);
    }
    g_readPos.store(rp);

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void AaudioErrorCallback(AAudioStream* stream, void* /*userData*/, aaudio_result_t error)
{
    LOGE("AAudio erro: %s (%d) — tentando reiniciar", AAudio_convertResultToText(error), error);
    // Fecha e reabre em caso de device disconnect
    AAudioStream_close(stream);
    g_aaStream = nullptr;
    Audio_Init(); // tenta reabrir
}

// ---------------------------------------------------------------------------
// Inicialização AAudio
// ---------------------------------------------------------------------------

static bool InitAAudio()
{
    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t res = AAudio_createStreamBuilder(&builder);
    if (res != AAUDIO_OK)
    {
        LOGE("AAudio_createStreamBuilder falhou: %s", AAudio_convertResultToText(res));
        return false;
    }

    AAudioStreamBuilder_setSampleRate       (builder, SAMPLE_RATE);
    AAudioStreamBuilder_setChannelCount     (builder, CHANNEL_COUNT);
    AAudioStreamBuilder_setFormat           (builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setPerformanceMode  (builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode      (builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setFramesPerDataCallback(builder, FRAMES_PER_CB);
    AAudioStreamBuilder_setDataCallback     (builder, AaudioDataCallback, nullptr);
    AAudioStreamBuilder_setErrorCallback    (builder, AaudioErrorCallback, nullptr);

    res = AAudioStreamBuilder_openStream(builder, &g_aaStream);
    AAudioStreamBuilder_delete(builder);

    if (res != AAUDIO_OK)
    {
        LOGE("AAudioStreamBuilder_openStream falhou: %s", AAudio_convertResultToText(res));
        g_aaStream = nullptr;
        return false;
    }

    int32_t actualRate = AAudioStream_getSampleRate(g_aaStream);
    int32_t actualBurst= AAudioStream_getFramesPerBurst(g_aaStream);
    LOGI("AAudio aberto: rate=%d burst=%d", actualRate, actualBurst);

    res = AAudioStream_requestStart(g_aaStream);
    if (res != AAUDIO_OK)
    {
        LOGE("AAudioStream_requestStart falhou: %s", AAudio_convertResultToText(res));
        AAudioStream_close(g_aaStream);
        g_aaStream = nullptr;
        return false;
    }

    LOGI("AAudio iniciado com sucesso");
    return true;
}

// ---------------------------------------------------------------------------
// Fallback: OpenSL ES (para dispositivos < API 26)
// ---------------------------------------------------------------------------

static SLObjectItf          g_slEngineObj   = nullptr;
static SLEngineItf          g_slEngine      = nullptr;
static SLObjectItf          g_slOutputMix   = nullptr;
static SLObjectItf          g_slPlayerObj   = nullptr;
static SLPlayItf            g_slPlay        = nullptr;

static bool InitOpenSLES()
{
    SLresult res;

    res = slCreateEngine(&g_slEngineObj, 0, nullptr, 0, nullptr, nullptr);
    if (res != SL_RESULT_SUCCESS) { LOGE("slCreateEngine falhou"); return false; }

    res = (*g_slEngineObj)->Realize(g_slEngineObj, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) { LOGE("slEngine Realize falhou"); return false; }

    res = (*g_slEngineObj)->GetInterface(g_slEngineObj, SL_IID_ENGINE, &g_slEngine);
    if (res != SL_RESULT_SUCCESS) { LOGE("GetInterface ENGINE falhou"); return false; }

    res = (*g_slEngine)->CreateOutputMix(g_slEngine, &g_slOutputMix, 0, nullptr, nullptr);
    if (res != SL_RESULT_SUCCESS) { LOGE("CreateOutputMix falhou"); return false; }

    res = (*g_slOutputMix)->Realize(g_slOutputMix, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) { LOGE("OutputMix Realize falhou"); return false; }

    LOGI("OpenSL ES inicializado (fallback)");
    return true;
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

bool Audio_Init()
{
    if (g_audioRunning.load()) return true;

    // Tenta AAudio primeiro (API 26+ — baixa latência)
    if (InitAAudio())
    {
        g_audioRunning.store(true);
        return true;
    }

    // Fallback: OpenSL ES
    LOGW("AAudio indisponivel, usando OpenSL ES");
    if (InitOpenSLES())
    {
        g_audioRunning.store(true);
        return true;
    }

    LOGE("Nenhum backend de audio disponivel");
    return false;
}

void Audio_Shutdown()
{
    g_audioRunning.store(false);
    g_audioPaused.store(false);

    if (g_aaStream)
    {
        AAudioStream_requestStop(g_aaStream);
        AAudioStream_close(g_aaStream);
        g_aaStream = nullptr;
        LOGI("AAudio encerrado");
    }

    if (g_slPlayerObj)
    {
        (*g_slPlayerObj)->Destroy(g_slPlayerObj);
        g_slPlayerObj = nullptr;
    }
    if (g_slOutputMix)
    {
        (*g_slOutputMix)->Destroy(g_slOutputMix);
        g_slOutputMix = nullptr;
    }
    if (g_slEngineObj)
    {
        (*g_slEngineObj)->Destroy(g_slEngineObj);
        g_slEngineObj = nullptr;
    }
    LOGI("OpenSL ES encerrado");
}

void Audio_OnPause()
{
    g_audioPaused.store(true);
    if (g_aaStream)
        AAudioStream_requestPause(g_aaStream);
    LOGI("Audio pausado");
}

void Audio_OnResume()
{
    if (g_aaStream)
        AAudioStream_requestStart(g_aaStream);
    g_audioPaused.store(false);
    LOGI("Audio retomado");
}

// Submete amostras PCM float ao buffer circular
void Audio_SubmitFrames(const float* frames, int32_t numFrames)
{
    if (!g_audioRunning.load() || g_audioPaused.load()) return;

    int wp = g_writePos.load();
    int32_t samples = numFrames * CHANNEL_COUNT;
    int32_t bufSize = FRAMES_PER_CB * CHANNEL_COUNT * 4;

    for (int32_t i = 0; i < samples; ++i)
    {
        g_mixBuffer[wp] = frames[i];
        wp = (wp + 1) % bufSize;
    }
    g_writePos.store(wp);
}

bool Audio_IsRunning() { return g_audioRunning.load(); }
