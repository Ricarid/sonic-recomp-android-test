#pragma once
#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// Detecção e perfil de GPU — Adreno, Mali, Dimensity, Exynos
// ---------------------------------------------------------------------------

enum class GpuVendor { Unknown, Qualcomm, ARM, Samsung, MediaTek, PowerVR };

enum class GraphicsQuality { Low, Medium, High, Ultra };

struct GpuProfile
{
    GpuVendor        vendor          = GpuVendor::Unknown;
    char             name[128]       = {};
    GraphicsQuality  quality         = GraphicsQuality::Medium;

    // Resolução de render (antes de upscale)
    float            renderScale     = 1.0f;   // 0.75 = 75% da resolução nativa

    // Sombras
    bool             shadowsEnabled  = true;
    int32_t          shadowMapSize   = 1024;

    // Efeitos
    bool             ssaoEnabled     = false;
    bool             particlesHigh   = false;
    float            drawDistance    = 300.f;

    // Cache de shaders
    bool             shaderCacheEnabled = true;
};

// ---------------------------------------------------------------------------
// Detecção baseada no nome do device Vulkan
// ---------------------------------------------------------------------------

inline GpuProfile DetectGpuProfile(const char* deviceName)
{
    GpuProfile p;
    strncpy(p.name, deviceName, sizeof(p.name) - 1);

    // -----------------------------------------------------------------------
    // Qualcomm Adreno
    // -----------------------------------------------------------------------
    if (strstr(deviceName, "Adreno"))
    {
        p.vendor = GpuVendor::Qualcomm;

        // Adreno 830+ (Snapdragon 8 Elite) — Ultra
        if (strstr(deviceName, "Adreno (TM) 830") ||
            strstr(deviceName, "Adreno (TM) 840"))
        {
            p.quality         = GraphicsQuality::Ultra;
            p.renderScale     = 1.0f;
            p.shadowMapSize   = 4096;
            p.ssaoEnabled     = true;
            p.particlesHigh   = true;
            p.drawDistance    = 600.f;
        }
        // Adreno 750/740 (Snapdragon 8 Gen 2/3) — High
        else if (strstr(deviceName, "Adreno (TM) 750") ||
                 strstr(deviceName, "Adreno (TM) 740"))
        {
            p.quality         = GraphicsQuality::High;
            p.renderScale     = 1.0f;
            p.shadowMapSize   = 2048;
            p.ssaoEnabled     = true;
            p.particlesHigh   = false;
            p.drawDistance    = 450.f;
        }
        // Adreno 730/720 (Snapdragon 8 Gen 1) — Medium-High
        else if (strstr(deviceName, "Adreno (TM) 730") ||
                 strstr(deviceName, "Adreno (TM) 720"))
        {
            p.quality         = GraphicsQuality::High;
            p.renderScale     = 0.9f;
            p.shadowMapSize   = 1024;
            p.ssaoEnabled     = false;
            p.drawDistance    = 350.f;
        }
        // Adreno 650/660 — Medium
        else
        {
            p.quality         = GraphicsQuality::Medium;
            p.renderScale     = 0.85f;
            p.shadowMapSize   = 1024;
            p.ssaoEnabled     = false;
            p.drawDistance    = 250.f;
        }
        return p;
    }

    // -----------------------------------------------------------------------
    // ARM Mali
    // -----------------------------------------------------------------------
    if (strstr(deviceName, "Mali"))
    {
        p.vendor = GpuVendor::ARM;

        // Mali-G710/G720 (Dimensity 9200+, Exynos 2400) — High
        if (strstr(deviceName, "Mali-G720") ||
            strstr(deviceName, "Mali-G715") ||
            strstr(deviceName, "Mali-G710"))
        {
            p.quality         = GraphicsQuality::High;
            p.renderScale     = 0.95f;
            p.shadowMapSize   = 2048;
            // Mali sofre mais com overdraw pesado — desativa SSAO por padrão
            p.ssaoEnabled     = false;
            p.particlesHigh   = false;
            p.drawDistance    = 400.f;
        }
        // Mali-G610/G68 (mid-range) — Medium
        else if (strstr(deviceName, "Mali-G610") ||
                 strstr(deviceName, "Mali-G68"))
        {
            p.quality         = GraphicsQuality::Medium;
            p.renderScale     = 0.80f;
            p.shadowMapSize   = 512;
            p.ssaoEnabled     = false;
            p.drawDistance    = 200.f;
        }
        // Mali legado — Low
        else
        {
            p.quality         = GraphicsQuality::Low;
            p.renderScale     = 0.70f;
            p.shadowMapSize   = 512;
            p.ssaoEnabled     = false;
            p.particlesHigh   = false;
            p.drawDistance    = 150.f;
        }
        return p;
    }

    // -----------------------------------------------------------------------
    // Samsung Xclipse (RDNA)
    // -----------------------------------------------------------------------
    if (strstr(deviceName, "Xclipse"))
    {
        p.vendor          = GpuVendor::Samsung;
        p.quality         = GraphicsQuality::High;
        p.renderScale     = 1.0f;
        p.shadowMapSize   = 2048;
        p.ssaoEnabled     = true;
        p.drawDistance    = 500.f;
        return p;
    }

    // Fallback conservador para GPUs desconhecidas
    p.quality     = GraphicsQuality::Low;
    p.renderScale = 0.75f;
    p.shadowMapSize = 512;
    return p;
}
