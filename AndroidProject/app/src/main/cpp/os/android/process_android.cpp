#include "process_android.h"
#include "logger_android.h"

#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <android/window.h>
#include <atomic>
#include <mutex>

// ---------------------------------------------------------------------------
// Estado global do ciclo de vida
// ---------------------------------------------------------------------------

static std::atomic<bool> g_appRunning{false};
static std::atomic<bool> g_appPaused{false};
static std::atomic<bool> g_windowReady{false};
static ANativeWindow*    g_nativeWindow = nullptr;
static std::mutex        g_windowMutex;

// ---------------------------------------------------------------------------
// Callbacks declarados externamente (implementados em vulkan/ e audio/)
// ---------------------------------------------------------------------------
extern void Vulkan_OnWindowCreated (ANativeWindow* window);
extern void Vulkan_OnWindowDestroyed();
extern void Vulkan_OnTrimMemory    (int level);
extern void Audio_OnPause          ();
extern void Audio_OnResume         ();

// ---------------------------------------------------------------------------
// Tratamento de comandos do android_native_app_glue
// ---------------------------------------------------------------------------

void Android_HandleCommand(android_app* app, int32_t cmd)
{
    switch (cmd)
    {
        // ------------------------------------------------------------------
        // Janela criada — pode inicializar superfície Vulkan
        // ------------------------------------------------------------------
        case APP_CMD_INIT_WINDOW:
        {
            std::lock_guard<std::mutex> lock(g_windowMutex);
            g_nativeWindow = app->window;
            if (g_nativeWindow)
            {
                LOGI("APP_CMD_INIT_WINDOW: janela disponivel %p", (void*)g_nativeWindow);
                Vulkan_OnWindowCreated(g_nativeWindow);
                g_windowReady.store(true);
            }
            break;
        }

        // ------------------------------------------------------------------
        // Janela destruída — libera superfície Vulkan antes de sair
        // ------------------------------------------------------------------
        case APP_CMD_TERM_WINDOW:
        {
            LOGI("APP_CMD_TERM_WINDOW: destruindo superficie Vulkan");
            g_windowReady.store(false);
            {
                std::lock_guard<std::mutex> lock(g_windowMutex);
                Vulkan_OnWindowDestroyed();
                g_nativeWindow = nullptr;
            }
            break;
        }

        // ------------------------------------------------------------------
        // Pause: para áudio, CPU throttle
        // ------------------------------------------------------------------
        case APP_CMD_PAUSE:
        {
            LOGI("APP_CMD_PAUSE");
            g_appPaused.store(true);
            Audio_OnPause();
            break;
        }

        // ------------------------------------------------------------------
        // Resume: retoma áudio
        // ------------------------------------------------------------------
        case APP_CMD_RESUME:
        {
            LOGI("APP_CMD_RESUME");
            g_appPaused.store(false);
            Audio_OnResume();
            break;
        }

        // ------------------------------------------------------------------
        // Foco ganho / perdido
        // ------------------------------------------------------------------
        case APP_CMD_GAINED_FOCUS:
        {
            LOGI("APP_CMD_GAINED_FOCUS");
            break;
        }
        case APP_CMD_LOST_FOCUS:
        {
            LOGI("APP_CMD_LOST_FOCUS");
            break;
        }

        // ------------------------------------------------------------------
        // Mudança de configuração (rotação, teclado, etc.)
        // ------------------------------------------------------------------
        case APP_CMD_CONFIG_CHANGED:
        {
            LOGI("APP_CMD_CONFIG_CHANGED: reconfigurando layout");
            // Re-query de display size para lidar com rotação
            if (app->window)
            {
                int32_t w = ANativeWindow_getWidth(app->window);
                int32_t h = ANativeWindow_getHeight(app->window);
                LOGI("  novo tamanho: %dx%d", w, h);
            }
            break;
        }

        // ------------------------------------------------------------------
        // App destruída
        // ------------------------------------------------------------------
        case APP_CMD_DESTROY:
        {
            LOGI("APP_CMD_DESTROY");
            g_appRunning.store(false);
            break;
        }

        // ------------------------------------------------------------------
        // Memória baixa — notifica Vulkan para descarregar recursos
        // ------------------------------------------------------------------
        case APP_CMD_LOW_MEMORY:
        {
            LOGW("APP_CMD_LOW_MEMORY: liberando recursos VRAM");
            Vulkan_OnTrimMemory(80 /* TRIM_MEMORY_COMPLETE */);
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Accessors de estado
// ---------------------------------------------------------------------------

bool Android_IsRunning()    { return g_appRunning.load();  }
bool Android_IsPaused()     { return g_appPaused.load();   }
bool Android_IsWindowReady(){ return g_windowReady.load(); }

ANativeWindow* Android_GetWindow()
{
    std::lock_guard<std::mutex> lock(g_windowMutex);
    return g_nativeWindow;
}

void Android_SetRunning(bool running)
{
    g_appRunning.store(running);
}
