#pragma once
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <android/native_window.h>

// ---------------------------------------------------------------------------
// Vulkan Android — superfície, swapchain e ciclo de vida
// ---------------------------------------------------------------------------

bool       Vulkan_Init();
void       Vulkan_OnWindowCreated (ANativeWindow* window);
void       Vulkan_OnWindowDestroyed();
void       Vulkan_OnTrimMemory    (int level);
void       Vulkan_Shutdown();

// Accessors
VkDevice   Vulkan_GetDevice();
VkInstance Vulkan_GetInstance();
VkQueue    Vulkan_GetQueue();
VkExtent2D Vulkan_GetExtent();
