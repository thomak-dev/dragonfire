#pragma once
#include "vulkan.h"
#include <SDL2/SDL.h>

class Graphics
{
public:
    Graphics(SDL_Window* window, std::string_view title, std::string_view engineName);
    ~Graphics();
    static vk::Instance GetVkInstance()
    {
        return instance;
    }
    void Render();
    void OnWindowSizeChanged();

private:
    static vk::Instance instance;
    std::vector<vk::CommandBuffer> graphicsCmdBuffers;
    std::vector<vk::CommandBuffer> acquireImageForPresentCmdBuffers;
    std::vector<vk::Fence> fences;
    std::vector<vk::Image> swapchainImages;

    vk::DebugUtilsMessengerEXT dbgMessenger;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Queue graphicsQueue;
    vk::Queue transferQueue;
    vk::Queue presentQueue;
    vk::SurfaceKHR surface;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::SwapchainKHR swapchain;
    vk::Semaphore renderingFinished;
    vk::Semaphore imageReady;
    vk::Semaphore imageAcquiredForPresent;
    vk::CommandPool graphicsCommandPool;
    vk::CommandPool presentCommandPool;

    vk::PresentModeKHR presentMode;
    uint32_t presentQueueFamily{};
    uint32_t graphicsQueueFamily{};
    uint32_t transferQueueFamily{};
    bool recreateSwapchain{};

    void AssignQueueFamiliyIndices(const std::vector<vk::QueueFamilyProperties>& queueFams);
    void DetermineSurfaceFormat();
    bool CreateSwapchain();
};
