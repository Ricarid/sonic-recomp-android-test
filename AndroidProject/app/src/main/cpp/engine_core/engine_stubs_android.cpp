// =============================================================================
// engine_stubs_android.cpp — Implementações que o JNI exige da engine
//
// O jni_bridge.cpp declara:
//   extern void InitializeAndroidFileSystem(AAssetManager* manager);
//   extern void PurgeVolatileMemory();
//
// Estas funções DEVERÃO ser implementadas pela engine real (UnleashedRecompLib)
// quando o submódulo for integrado.
//
// Por enquanto fornecemos stubs que:
//   - InitializeAndroidFileSystem: armazena o AAssetManager e configura os
//     caminhos de armazenamento via Android_SetStoragePaths
//   - PurgeVolatileMemory: libera caches não críticos (no stub, apenas loga)
//
// Quando a engine real for integrada, SUBSTITUA este arquivo ou remova-o do
// CMakeLists.txt e forneça as implementações reais no engine_core.
// =============================================================================

#include <android/asset_manager.h>
#include "../os/android/logger_android.h"

// ---------------------------------------------------------------------------
// AAssetManager global (usado por outros módulos para leitura de assets)
// ---------------------------------------------------------------------------
static AAssetManager* g_assetManager = nullptr;

AAssetManager* Engine_GetAssetManager()
{
    return g_assetManager;
}

// ---------------------------------------------------------------------------
// InitializeAndroidFileSystem
//
// Chamada pelo jni_bridge quando o AssetManager Java fica disponível.
// Responsabilidades reais (quando engine integrada):
//   - Montar sistema de arquivos virtual sobre AAssetManager
//   - Registrar caminhos game/, update/, dlc/ na camada kernel/io/file_system
//   - Inicializar FileSystem::ResolvePath() para Android
// ---------------------------------------------------------------------------

extern "C" void InitializeAndroidFileSystem(AAssetManager* manager)
{
    g_assetManager = manager;

    if (!manager)
    {
        LOGE("InitializeAndroidFileSystem: manager nulo!");
        return;
    }

    LOGI("InitializeAndroidFileSystem: AAssetManager=%p", (void*)manager);

    // TODO (integração engine real):
    //   FileSystem::SetAndroidAssetManager(manager);
    //   FileSystem::MountPath("game",   GetGamePath() / "game");
    //   FileSystem::MountPath("update", GetGamePath() / "update");
    //   FileSystem::MountPath("D",      GetGamePath() / "game");
    //
    //   for (cada DLC em GetGamePath() / "dlc")
    //       FileSystem::MountDLC(dlcName, dlcPath);

    LOGI("InitializeAndroidFileSystem: OK (stub — engine real não integrada)");
}

// ---------------------------------------------------------------------------
// PurgeVolatileMemory
//
// Chamada pelo jni_bridge em nativeOnTrimMemory.
// Responsabilidade real (quando engine integrada):
//   - Liberar caches de textura não essenciais
//   - Descarregar shaders não usados recentemente
//   - Liberar buffers de streaming ociosos
// ---------------------------------------------------------------------------

extern "C" void PurgeVolatileMemory()
{
    LOGW("PurgeVolatileMemory: liberando recursos voláteis (stub)");

    // TODO (integração engine real):
    //   TextureCache::Evict(TextureCache::Priority::Low);
    //   ShaderCache::EvictUnused();
    //   StreamingBuffer::Flush();
}
