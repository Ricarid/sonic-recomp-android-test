#pragma once

// ---------------------------------------------------------------------------
// Logger Android — wrappers sobre __android_log_print
// ---------------------------------------------------------------------------

void LogInfo   (const char* fmt, ...);
void LogWarn   (const char* fmt, ...);
void LogError  (const char* fmt, ...);
void LogDebug  (const char* fmt, ...);
void LogVerbose(const char* fmt, ...);

// Macros de conveniência
#define LOGI(...)  LogInfo(__VA_ARGS__)
#define LOGW(...)  LogWarn(__VA_ARGS__)
#define LOGE(...)  LogError(__VA_ARGS__)
#define LOGD(...)  LogDebug(__VA_ARGS__)
#define LOGV(...)  LogVerbose(__VA_ARGS__)
