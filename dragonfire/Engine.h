#pragma once
#include <string_view>
#include <optional>
#include <SDL2/SDL.h>
#include "common.h"
class Engine
{
public:
    Engine(std::string_view title);
    ~Engine();
    void Run();

private:
    SDL_Window* window{};
    vk::Instance instance{};
    vk::SurfaceKHR surface{};
    vk::DebugUtilsMessengerEXT dbgMessenger{};
    vk::PhysicalDevice physicalDevice{};
    vk::Device device{};
    vk::Queue graphicsQueue{};
    vk::Queue transferQueue{};
    vk::Queue presentQueue{};
    uint32_t indexPresent;
    uint32_t indexGraphics;
    uint32_t indexTransfer;
    vk::SurfaceFormatKHR surfaceFormat{};
    vk::PresentModeKHR presentMode{};
    vk::SwapchainKHR swapchain{};
    std::vector<vk::Image> swapchainImages;
    vk::Semaphore renderingFinished{};
    vk::Semaphore imageReady{};
    std::array<vk::CommandPool, 2> graphicsCommandPools;
    std::array<vk::CommandBuffer, 2> graphicsCmdBuffers;
    std::array<vk::Fence, 2> fences;

    void AssignQueueFamiliyIndices(const std::vector<vk::QueueFamilyProperties>& queueFams);
    void DetermineSurfaceFormat();
    bool CreateSwapchain();
};
