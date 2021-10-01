#pragma once
#include <optional>
#include <string_view>

#include "vulkan.h"
#include <SDL2/SDL.h>

class Engine
{
  public:
    Engine(std::string_view title);
    ~Engine();
    void Run();
    static vk::Instance GetVkInstance()
    {
        return instance;
    }

  private:
    SDL_Window* window{};
    static vk::Instance instance;
    vk::DebugUtilsMessengerEXT dbgMessenger;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;

    vk::Queue graphicsQueue;
    vk::Queue transferQueue;
    vk::Queue presentQueue;
    uint32_t presentQueueFamily{};
    uint32_t graphicsQueueFamily{};
    uint32_t transferQueueFamily{};

    vk::SurfaceKHR surface;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::PresentModeKHR presentMode;
    vk::SwapchainKHR swapchain;
    std::vector<vk::Image> swapchainImages;

    vk::Semaphore renderingFinished;
    vk::Semaphore imageReady;
    vk::Semaphore imageAcquiredForPresent;
    vk::CommandPool graphicsCommandPool;
    vk::CommandPool presentCommandPool;
    std::vector<vk::CommandBuffer> graphicsCmdBuffers;
    std::vector<vk::Fence> fences;
    std::vector<vk::CommandBuffer> acquireImageForPresentCmdBuffers;

    void AssignQueueFamiliyIndices(const std::vector<vk::QueueFamilyProperties>& queueFams);
    void DetermineSurfaceFormat();
    bool CreateSwapchain();
};
