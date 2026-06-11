#include "logger_android.h"
#include <android/log.h>
#include <cstdarg>
#include <cstdio>

#define LOG_TAG "SonicPort"
#define LOG_BUF_SIZE 1024

// ---------------------------------------------------------------------------
// Implementação do logger para Android
// ---------------------------------------------------------------------------

void LogInfo(const char* fmt, ...)
{
    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
}

void LogWarn(const char* fmt, ...)
{
    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "%s", buf);
}

void LogError(const char* fmt, ...)
{
    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", buf);
}

void LogDebug(const char* fmt, ...)
{
#ifdef SONIC_DEBUG
    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", buf);
#else
    (void)fmt;
#endif
}

void LogVerbose(const char* fmt, ...)
{
#ifdef SONIC_VERBOSE
    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "%s", buf);
#else
    (void)fmt;
#endif
}
