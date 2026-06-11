#pragma once

// =============================================================================
// paths_android.h — Caminhos de armazenamento para Android
//
// Equivale a user/paths.h + user/paths.cpp do PC.
// No Android, não existe executável em disco; os caminhos são baseados em:
//   - Internal storage via Context.getFilesDir()   → passado pelo JNI
//   - External storage via Context.getExternalFilesDir()
//   - Assets via AAssetManager
//
// Os caminhos são inicializados em Android_SetStoragePaths() chamado do JNI.
// =============================================================================

#include <string>
#include <filesystem>

// ---------------------------------------------------------------------------
// Inicialização (chamada do jni_bridge após ter o Context Java)
// ---------------------------------------------------------------------------

void Android_SetStoragePaths(const std::string& internalPath,
                              const std::string& externalPath);

// ---------------------------------------------------------------------------
// Equivalentes às funções globais do PC
// ---------------------------------------------------------------------------

// Diretório raiz onde o jogo foi instalado (assets extraídos ou montados)
// No Android: external storage ou internal storage, dependendo do usuário
std::filesystem::path GetGamePath();

// Diretório onde configs e saves do usuário ficam guardados
// No Android: internal storage (privado ao app)
const std::filesystem::path& GetUserPath();

// Caminho do arquivo de save
std::filesystem::path GetSavePath(bool checkForMods);
std::filesystem::path GetSaveFilePath(bool checkForMods);

// ---------------------------------------------------------------------------
// "Executable root" no Android → diretório interno do app (sem sentido real
// de executável, mas o PC usa isso para portable.txt e outros recursos)
// ---------------------------------------------------------------------------
std::filesystem::path GetExecutableRoot();
