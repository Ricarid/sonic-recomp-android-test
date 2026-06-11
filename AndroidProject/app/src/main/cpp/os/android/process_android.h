#pragma once
#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// Lifecycle Android
// ---------------------------------------------------------------------------

void           Android_HandleCommand(android_app* app, int32_t cmd);
bool           Android_IsRunning();
bool           Android_IsPaused();
bool           Android_IsWindowReady();
ANativeWindow* Android_GetWindow();
void           Android_SetRunning(bool running);
