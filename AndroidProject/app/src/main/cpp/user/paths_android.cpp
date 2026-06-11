// =============================================================================
// paths_android.cpp — Caminhos de armazenamento para Android
//
// Equivale a user/paths.cpp do PC.
//
// Estrutura de diretórios esperada no dispositivo:
//
//   External (/sdcard/Android/data/com.sega.sonicunleashed/files/):
//     game/       ← arquivos do jogo extraídos (game/, update/, dlc/)
//     mods/       ← mods do usuário
//
//   Internal (app-private):
//     save/       ← saves
//     config/     ← configurações
//     shaders/    ← pipeline cache de shaders
//
// =============================================================================

#include "paths_android.h"
#include "../os/android/logger_android.h"

#include <filesystem>
#include <string>

// ---------------------------------------------------------------------------
// Estado global inicializado pelo JNI
// ---------------------------------------------------------------------------

static std::filesystem::path g_internalPath;  // Context.getFilesDir()
static std::filesystem::path g_externalPath;  // Context.getExternalFilesDir(null)
static bool                  g_pathsSet = false;

void Android_SetStoragePaths(const std::string& internalPath,
                              const std::string& externalPath)
{
    g_internalPath = internalPath;
    g_externalPath = externalPath;
    g_pathsSet     = true;

    // Garante que os diretórios existem
    std::error_code ec;
    std::filesystem::create_directories(g_internalPath / "save",    ec);
    std::filesystem::create_directories(g_internalPath / "config",  ec);
    std::filesystem::create_directories(g_internalPath / "shaders", ec);
    std::filesystem::create_directories(g_externalPath / "game",    ec);
    std::filesystem::create_directories(g_externalPath / "mods",    ec);

    LOGI("Paths inicializados:");
    LOGI("  internal : %s", internalPath.c_str());
    LOGI("  external : %s", externalPath.c_str());
}

// ---------------------------------------------------------------------------
// GetGamePath — onde os assets do jogo estão
// No Android: external storage para que o usuário possa colocar os arquivos
// ---------------------------------------------------------------------------

std::filesystem::path GetGamePath()
{
    if (!g_pathsSet)
    {
        LOGE("GetGamePath: paths não inicializados! JNI SetStoragePaths não foi chamado.");
        return std::filesystem::path("/sdcard/Android/data/com.sega.sonicunleashed/files");
    }
    return g_externalPath;
}

// ---------------------------------------------------------------------------
// GetUserPath — configs e dados do app (privado)
// ---------------------------------------------------------------------------

const std::filesystem::path& GetUserPath()
{
    if (!g_pathsSet)
    {
        LOGE("GetUserPath: paths não inicializados!");
        static std::filesystem::path fallback = "/data/data/com.sega.sonicunleashed/files";
        return fallback;
    }
    return g_internalPath;
}

// ---------------------------------------------------------------------------
// GetExecutableRoot — "raiz do executável"
// No Android mapeamos para o internal path (onde ficam recursos do app)
// ---------------------------------------------------------------------------

std::filesystem::path GetExecutableRoot()
{
    return GetUserPath();
}

// ---------------------------------------------------------------------------
// GetSavePath / GetSaveFilePath
// ---------------------------------------------------------------------------

std::filesystem::path GetSavePath(bool /*checkForMods*/)
{
    // TODO: integrar ModLoader quando disponível
    return GetUserPath() / "save";
}

std::filesystem::path GetSaveFilePath(bool checkForMods)
{
    return GetSavePath(checkForMods) / "SYS-DATA";
}
