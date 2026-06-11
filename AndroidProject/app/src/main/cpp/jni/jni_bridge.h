#pragma once

// =============================================================================
// jni_bridge.h — Declarações públicas da ponte JNI
// =============================================================================

#include <jni.h>
#include <android/asset_manager.h>

// Retorna o AAssetManager global registrado pelo Java
AAssetManager* JNI_GetAssetManager();
