#pragma once

constexpr uint32_t swapchainImagesDesired = 3;

class GraphicsBase
{
public:
    GraphicsBase(SDL_Window* window, std::string_view title);
    GraphicsBase() = delete;
    GraphicsBase(GraphicsBase&) = delete;
    GraphicsBase(GraphicsBase&&) = delete;
    GraphicsBase& operator=(GraphicsBase&) = delete;
    GraphicsBase& operator=(GraphicsBase&&) = delete;

    virtual ~GraphicsBase();

    vk::Instance VkInstance() const noexcept { return vkInstance; }
    VmaAllocator Allocator() const noexcept { return allocator; }
    vk::Device Device() const noexcept { return device; }
    uint32_t PresentQueueFamily() const noexcept { return presentQueueFamily; }
    uint32_t GraphicsQueueFamily() const noexcept { return graphicsQueueFamily; }
    uint32_t TransferQueueFamily() const noexcept { return transferQueueFamily; }

protected:
    vk::Instance vkInstance;
    vk::DebugUtilsMessengerEXT dbgMessenger;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Queue graphicsQueue;
    vk::Queue transferQueue;
    vk::Queue presentQueue;
    vk::SurfaceKHR surface;
    VmaAllocator allocator{};

    uint32_t presentQueueFamily{};
    uint32_t graphicsQueueFamily{};
    uint32_t transferQueueFamily{};

    static void IgnoreVkMessage(uint32_t messageId) noexcept;
    static void UnignoreVkMessage(uint32_t messageId) noexcept;

private:
    void AssignQueueFamiliyIndices(const std::vector<vk::QueueFamilyProperties>& queueFams);
};
