// =============================================================================
// jni_bridge.cpp — Ponte Java ↔ C++
//
// CORREÇÕES aplicadas:
//   1. nativeSurfaceChanged implementado corretamente (reconstrói swapchain)
//   2. nativeSetStoragePaths adicionado (inicializa paths_android)
//   3. nativeOnKeyEvent / nativeOnMotionEvent para controles físicos
//   4. Vibração orientada pelo estado do HID (não só duração fixa)
//   5. nativeGetSavePath usa GetUserPath() ao invés de JNI hardcoded
// =============================================================================

#include "jni_bridge.h"
#include "../os/android/logger_android.h"
#include "../os/android/process_android.h"
#include "../audio/audio_android.h"
#include "../vulkan/vulkan_android.h"
#include "../input/touch_input.h"
#include "../input/virtual_gamepad.h"
#include "../hid/hid_android.h"
#include "../user/paths_android.h"

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Forward declarations — implementados pelo engine_core (ou stubs)
// ---------------------------------------------------------------------------

extern "C" void InitializeAndroidFileSystem(AAssetManager* manager);
extern "C" void PurgeVolatileMemory();

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static JavaVM*        g_jvm          = nullptr;
static AAssetManager* g_assetManager = nullptr;

// ---------------------------------------------------------------------------
// JNI_OnLoad
// ---------------------------------------------------------------------------

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    g_jvm = vm;
    LOGI("JNI_OnLoad: JavaVM registrada");
    return JNI_VERSION_1_6;
}

// ---------------------------------------------------------------------------
// Helper: obtém JNIEnv para a thread atual
// ---------------------------------------------------------------------------

static JNIEnv* GetEnv()
{
    if (!g_jvm) return nullptr;
    JNIEnv* env = nullptr;
    int ret = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (ret == JNI_EDETACHED)
    {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    }
    return env;
}

// ---------------------------------------------------------------------------
// Assets + filesystem
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeSetAssetManager(
    JNIEnv* env, jobject /*thiz*/, jobject assetManager)
{
    g_assetManager = AAssetManager_fromJava(env, assetManager);
    LOGI("AssetManager registrado: %p", (void*)g_assetManager);

    if (g_assetManager)
    {
        InitializeAndroidFileSystem(g_assetManager);
        LOGI("Sistema de arquivos virtual inicializado via AssetManager");
    }
    else
    {
        LOGE("Erro ao obter ponteiro do AssetManager via JNI");
    }
}

// ---------------------------------------------------------------------------
// NOVO: inicializa caminhos de armazenamento
// Deve ser chamado de MainActivity.onCreate() antes de nativeOnCreate()
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeSetStoragePaths(
    JNIEnv* env, jobject /*thiz*/,
    jstring internalPath, jstring externalPath)
{
    const char* internal = env->GetStringUTFChars(internalPath, nullptr);
    const char* external = env->GetStringUTFChars(externalPath, nullptr);

    Android_SetStoragePaths(
        internal ? internal : "",
        external ? external : "");

    if (internal) env->ReleaseStringUTFChars(internalPath, internal);
    if (external) env->ReleaseStringUTFChars(externalPath, external);
}

AAssetManager* JNI_GetAssetManager() { return g_assetManager; }

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnCreate(
    JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeOnCreate");
    Vulkan_Init();
    Audio_Init();
}

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnDestroy(
    JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeOnDestroy");
    Audio_Shutdown();
    Vulkan_Shutdown();
    Touch_Reset();
    VirtualGamepad_Reset();
}

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnPause(
    JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeOnPause");
    Audio_OnPause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnResume(
    JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeOnResume");
    Audio_OnResume();
}

// ---------------------------------------------------------------------------
// Gerenciamento de memória
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnTrimMemory(
    JNIEnv* /*env*/, jobject /*thiz*/, jint level)
{
    LOGW("nativeOnTrimMemory: level=%d", level);
    PurgeVolatileMemory();
    Vulkan_OnTrimMemory(level);

    if (level >= 60 /*TRIM_MEMORY_MODERATE*/)
    {
        LOGW("TrimMemory moderado — pausando audio");
        Audio_OnPause();
    }
}

// ---------------------------------------------------------------------------
// Surface Vulkan — CORRIGIDO: nativeSurfaceChanged reconstrói o swapchain
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeSurfaceCreated(
    JNIEnv* env, jobject /*thiz*/, jobject surface)
{
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    LOGI("nativeSurfaceCreated: window=%p", (void*)window);
    Vulkan_OnWindowCreated(window);
    ANativeWindow_release(window);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeSurfaceDestroyed(
    JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeSurfaceDestroyed");
    Vulkan_OnWindowDestroyed();
}

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeSurfaceChanged(
    JNIEnv* env, jobject /*thiz*/, jobject surface, jint width, jint height)
{
    LOGI("nativeSurfaceChanged: %dx%d", width, height);

    // CORREÇÃO: reconstrói swapchain quando o tamanho da surface muda
    // (rotação de tela, split-screen, redimensionamento)
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window)
    {
        // Destroi surface/swapchain antigos e cria novos com o novo tamanho
        Vulkan_OnWindowDestroyed();
        Vulkan_OnWindowCreated(window);
        ANativeWindow_release(window);
        LOGI("nativeSurfaceChanged: swapchain reconstruída (%dx%d)", width, height);
    }
    else
    {
        LOGE("nativeSurfaceChanged: ANativeWindow_fromSurface retornou nulo");
    }
}

// ---------------------------------------------------------------------------
// Input — toque
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnTouchEvent(
    JNIEnv* /*env*/, jobject /*thiz*/,
    jint action, jint pointerId, jfloat x, jfloat y)
{
    Touch_OnEvent(action, pointerId, x, y);
}

// ---------------------------------------------------------------------------
// Input — controle físico (NOVO)
// Permite que a Activity repasse eventos KeyEvent/MotionEvent para o C++
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnKeyEvent(
    JNIEnv* /*env*/, jobject /*thiz*/, jint action, jint keyCode)
{
    HID_OnKeyEvent(action, keyCode);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnMotionEvent(
    JNIEnv* /*env*/, jobject /*thiz*/,
    jfloat leftX, jfloat leftY,
    jfloat rightX, jfloat rightY,
    jfloat lt, jfloat rt)
{
    HID_OnMotionEvent(leftX, leftY, rightX, rightY, lt, rt);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnGamepadConnected(
    JNIEnv* /*env*/, jobject /*thiz*/, jboolean connected)
{
    HID_OnGamepadConnected(connected == JNI_TRUE);
}

// ---------------------------------------------------------------------------
// Back pressed
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeOnBackPressed(
    JNIEnv* /*env*/, jobject /*thiz*/)
{
    LOGI("nativeOnBackPressed — abrindo menu de pausa");
    // TODO: enfileirar evento de pausa para a engine quando integrada
}

// ---------------------------------------------------------------------------
// Vibração — agora sincroniza com o estado do HID
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeVibrate(
    JNIEnv* env, jobject thiz, jlong durationMs)
{
    jclass    cls = env->GetObjectClass(thiz);
    jmethodID mid = env->GetMethodID(cls, "performVibrate", "(J)V");
    if (mid) env->CallVoidMethod(thiz, mid, durationMs);
}

// Chamado pelo game loop para enviar vibração atual do HID ao Java
extern "C" JNIEXPORT void JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeUpdateVibration(
    JNIEnv* env, jobject thiz)
{
    uint16_t left  = HID_GetVibrationLeft();
    uint16_t right = HID_GetVibrationRight();

    if (left > 0 || right > 0)
    {
        // Converte intensidade (0-65535) em duração de pulso (ms)
        jlong duration = (jlong)(std::max(left, right) / 655);  // 0-100ms
        if (duration > 0)
        {
            jclass    cls = env->GetObjectClass(thiz);
            jmethodID mid = env->GetMethodID(cls, "performVibrate", "(J)V");
            if (mid) env->CallVoidMethod(thiz, mid, duration);
        }
    }
}

// ---------------------------------------------------------------------------
// Permissões
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jboolean JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeCheckStoragePermission(
    JNIEnv* env, jobject thiz)
{
    jclass    cls = env->GetObjectClass(thiz);
    jmethodID mid = env->GetMethodID(cls, "hasStoragePermission", "()Z");
    if (mid) return env->CallBooleanMethod(thiz, mid);
    return JNI_FALSE;
}

// ---------------------------------------------------------------------------
// Save path — CORRIGIDO: usa GetUserPath() ao invés de JNI hardcoded
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT jstring JNICALL
Java_com_sega_sonicunleashed_MainActivity_nativeGetSavePath(
    JNIEnv* env, jobject /*thiz*/)
{
    std::string savePath = GetSavePath(false).string();
    LOGI("nativeGetSavePath: %s", savePath.c_str());
    return env->NewStringUTF(savePath.c_str());
}
