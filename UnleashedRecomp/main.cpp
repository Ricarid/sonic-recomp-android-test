#include "file_system.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "os/android/logger_android.h"

extern AAssetManager* g_AssetManager;
extern std::string g_ExternalFilesDir;

void* LoadGameAsset(const std::string& filename) {
    if (!g_AssetManager) return nullptr;
    
    // 1. Tenta carregar do cache extraido (ExternalFilesDir)
    std::string externalPath = g_ExternalFilesDir + "/" + filename;
    FILE* f = fopen(externalPath.c_str(), "rb");
    if (f) {
        fclose(f);
        return nullptr; 
    }

    // 2. Fallback direto do APK (AAssetManager)
    AAsset* asset = AAssetManager_open(g_AssetManager, filename.c_str(), AASSET_MODE_BUFFER);
    if (!asset) return nullptr;
    
    off_t size = AAsset_getLength(asset);
    char* buffer = new char[size];
    AAsset_read(asset, buffer, size);
    AAsset_close(asset);
    
    return buffer;
}