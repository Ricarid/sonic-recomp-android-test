// =============================================================================
// os_android.cpp — Implementações Android para os::user, os::version,
//                  os::media, os::registry
//
// Equivale em conjunto a:
//   os/linux/user_linux.cpp
//   os/linux/version_linux.cpp
//   os/linux/media_linux.cpp
//   (+ stubs de registry que o PC implementa via .inl)
// =============================================================================

#include <os/user.h>
#include <os/version.h>
#include <os/media.h>

#include "logger_android.h"

#include <android/api-level.h>
#include <sys/system_properties.h>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// os::user
// ---------------------------------------------------------------------------

bool os::user::IsDarkTheme()
{
    // Android não expõe tema diretamente via NDK.
    // Retorna false (tema claro) como default seguro.
    return false;
}

// ---------------------------------------------------------------------------
// os::version
// ---------------------------------------------------------------------------

os::version::OSVersion os::version::GetOSVersion()
{
    os::version::OSVersion ver{};

    // Lê a versão do Android via propriedade do sistema
    char sdkStr[PROP_VALUE_MAX] = {};
    __system_property_get("ro.build.version.sdk", sdkStr);

    char releaseStr[PROP_VALUE_MAX] = {};
    __system_property_get("ro.build.version.release", releaseStr);

    // Parse "major.minor.patch" do release string
    int major = 0, minor = 0, patch = 0;
    if (sscanf(releaseStr, "%d.%d.%d", &major, &minor, &patch) >= 1)
    {
        ver.Major = static_cast<uint32_t>(major);
        ver.Minor = static_cast<uint32_t>(minor);
        ver.Build = static_cast<uint32_t>(atoi(sdkStr)); // API level como build
    }

    LOGI("OS Version: Android %s (API %s)", releaseStr, sdkStr);
    return ver;
}

// ---------------------------------------------------------------------------
// os::media
// ---------------------------------------------------------------------------

bool os::media::IsExternalMediaPlaying()
{
    // Android NDK não oferece esta funcionalidade diretamente.
    // Retorna false: o jogo não vai pausar música por causa de mídia externa.
    return false;
}
