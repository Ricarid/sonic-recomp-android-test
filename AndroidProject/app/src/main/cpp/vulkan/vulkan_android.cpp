#include "vulkan_android.h"
#include "../os/android/logger_android.h"

#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <android/native_window.h>
#include <vector>
#include <cstring>

// ---------------------------------------------------------------------------
// Handles Vulkan
// ---------------------------------------------------------------------------

static VkInstance               g_instance       = VK_NULL_HANDLE;
static VkPhysicalDevice         g_physDevice     = VK_NULL_HANDLE;
static VkDevice                 g_device         = VK_NULL_HANDLE;
static VkQueue                  g_graphicsQueue  = VK_NULL_HANDLE;
static uint32_t                 g_queueFamily    = 0;
static VkSurfaceKHR             g_surface        = VK_NULL_HANDLE;
static VkSwapchainKHR           g_swapchain      = VK_NULL_HANDLE;
static std::vector<VkImage>     g_swapImages;
static std::vector<VkImageView> g_swapViews;
static VkFormat                 g_swapFormat     = VK_FORMAT_B8G8R8A8_UNORM;
static VkExtent2D               g_swapExtent     = {0, 0};
static bool                     g_initialized    = false;

// ---------------------------------------------------------------------------
// Helpers internos
// ---------------------------------------------------------------------------

static bool CheckLayer(const char* name)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (auto& l : layers)
        if (strcmp(l.layerName, name) == 0) return true;
    return false;
}

static bool CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "SonicUnleashedPort";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "SonicEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };

    std::vector<const char*> validationLayers;
#ifdef SONIC_DEBUG
    if (CheckLayer("VK_LAYER_KHRONOS_validation"))
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = 2;
    ci.ppEnabledExtensionNames = extensions;
    ci.enabledLayerCount       = (uint32_t)validationLayers.size();
    ci.ppEnabledLayerNames     = validationLayers.data();

    if (vkCreateInstance(&ci, nullptr, &g_instance) != VK_SUCCESS)
    {
        LOGE("Vulkan: vkCreateInstance falhou");
        return false;
    }
    LOGI("Vulkan: instancia criada");
    return true;
}

static bool PickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(g_instance, &count, nullptr);
    if (count == 0) { LOGE("Vulkan: nenhuma GPU encontrada"); return false; }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(g_instance, &count, devices.data());

    // Prefere GPU dedicada (Adreno/Mali); aceita qualquer outra
    for (auto& dev : devices)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        LOGI("Vulkan: GPU encontrada: %s", props.deviceName);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            g_physDevice = dev;
            LOGI("Vulkan: selecionando %s", props.deviceName);
            break;
        }
    }
    if (g_physDevice == VK_NULL_HANDLE) g_physDevice = devices[0];
    return true;
}

static bool CreateDevice()
{
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physDevice, &qfCount, qfs.data());

    g_queueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < qfCount; ++i)
        if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            { g_queueFamily = i; break; }

    if (g_queueFamily == UINT32_MAX) { LOGE("Vulkan: queue grafica nao encontrada"); return false; }

    float prio = 1.f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = g_queueFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    const char* devExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = devExtensions;

    if (vkCreateDevice(g_physDevice, &dci, nullptr, &g_device) != VK_SUCCESS)
    {
        LOGE("Vulkan: vkCreateDevice falhou");
        return false;
    }
    vkGetDeviceQueue(g_device, g_queueFamily, 0, &g_graphicsQueue);
    LOGI("Vulkan: device criado, queue=%u", g_queueFamily);
    return true;
}

static bool CreateSwapchain(ANativeWindow* window)
{
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physDevice, g_surface, &caps);

    g_swapExtent = caps.currentExtent;
    if (g_swapExtent.width == UINT32_MAX)
    {
        g_swapExtent.width  = (uint32_t)ANativeWindow_getWidth(window);
        g_swapExtent.height = (uint32_t)ANativeWindow_getHeight(window);
    }

    uint32_t imageCount = std::max(caps.minImageCount + 1,
                                   (uint32_t)3); // triple buffering
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = g_surface;
    sci.minImageCount    = imageCount;
    sci.imageFormat      = g_swapFormat;
    sci.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent      = g_swapExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = VK_PRESENT_MODE_MAILBOX_KHR; // latência mínima
    sci.clipped          = VK_TRUE;

    if (vkCreateSwapchainKHR(g_device, &sci, nullptr, &g_swapchain) != VK_SUCCESS)
    {
        LOGE("Vulkan: vkCreateSwapchainKHR falhou");
        return false;
    }

    vkGetSwapchainImagesKHR(g_device, g_swapchain, &imageCount, nullptr);
    g_swapImages.resize(imageCount);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &imageCount, g_swapImages.data());

    g_swapViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo ivci{};
        ivci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image                           = g_swapImages[i];
        ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format                          = g_swapFormat;
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(g_device, &ivci, nullptr, &g_swapViews[i]) != VK_SUCCESS)
            LOGE("Vulkan: ImageView %u falhou", i);
    }

    LOGI("Vulkan: swapchain %ux%u images=%u", g_swapExtent.width, g_swapExtent.height, imageCount);
    return true;
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

bool Vulkan_Init()
{
    if (!CreateInstance()) return false;
    if (!PickPhysicalDevice()) return false;
    if (!CreateDevice()) return false;
    g_initialized = true;
    LOGI("Vulkan: inicializacao base concluida");
    return true;
}

void Vulkan_OnWindowCreated(ANativeWindow* window)
{
    if (!g_initialized)
    {
        LOGE("Vulkan: Vulkan_Init nao foi chamado antes de OnWindowCreated");
        return;
    }

    // ------------------------------------------------------------------
    // CORREÇÃO CRÍTICA: cria a superfície Android corretamente
    // ------------------------------------------------------------------
    VkAndroidSurfaceCreateInfoKHR surfaceCI{};
    surfaceCI.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCI.window = window;

    VkResult res = vkCreateAndroidSurfaceKHR(g_instance, &surfaceCI, nullptr, &g_surface);
    if (res != VK_SUCCESS)
    {
        LOGE("Vulkan: vkCreateAndroidSurfaceKHR falhou (VkResult=%d)", res);
        return;
    }
    LOGI("Vulkan: superficie Android criada %p", (void*)g_surface);

    // Verifica suporte a apresentacao
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_physDevice, g_queueFamily, g_surface, &supported);
    if (!supported)
    {
        LOGE("Vulkan: queue nao suporta apresentacao na superficie");
        return;
    }

    CreateSwapchain(window);
}

void Vulkan_OnWindowDestroyed()
{
    if (g_device)
    {
        vkDeviceWaitIdle(g_device);

        for (auto& view : g_swapViews)
            vkDestroyImageView(g_device, view, nullptr);
        g_swapViews.clear();
        g_swapImages.clear();

        if (g_swapchain)
        {
            vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
            g_swapchain = VK_NULL_HANDLE;
        }
    }
    if (g_instance && g_surface)
    {
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        g_surface = VK_NULL_HANDLE;
    }
    LOGI("Vulkan: superficie destruida");
}

void Vulkan_OnTrimMemory(int level)
{
    LOGW("Vulkan: TrimMemory level=%d — liberando recursos opcionais", level);
    // Aqui: descarregar shader cache, texturas de baixa prioridade, etc.
    // A implementacao real depende do gerenciador de recursos do engine.
    if (level >= 80) // TRIM_MEMORY_COMPLETE
    {
        LOGW("Vulkan: nivel critico — liberando caches de shader");
        // ShaderCache_Evict(); — a ser integrado com o engine_core
    }
}

void Vulkan_Shutdown()
{
    Vulkan_OnWindowDestroyed();

    if (g_device)  { vkDestroyDevice(g_device, nullptr);   g_device = VK_NULL_HANDLE; }
    if (g_instance){ vkDestroyInstance(g_instance, nullptr); g_instance = VK_NULL_HANDLE; }
    g_initialized = false;
    LOGI("Vulkan: encerrado");
}

VkDevice   Vulkan_GetDevice()   { return g_device;   }
VkInstance Vulkan_GetInstance() { return g_instance; }
VkQueue    Vulkan_GetQueue()    { return g_graphicsQueue; }
VkExtent2D Vulkan_GetExtent()   { return g_swapExtent; }
