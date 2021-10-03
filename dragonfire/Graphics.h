#pragma once

#include "GraphicsBase.h"

class OriginGizmo;
class Graphics : public GraphicsBase
{
public:
    Graphics(SDL_Window* window, std::string_view title, std::string_view engineName);
    Graphics() = delete;
    Graphics(Graphics&) = delete;
    Graphics(Graphics&&) = delete;
    Graphics& operator=(Graphics&) = delete;
    Graphics& operator=(Graphics&&) = delete;
    ~Graphics();

    static Graphics& Instance() noexcept { return *dynamic_cast<Graphics*>(instance); }
    uint32_t Width() const noexcept { return width; }
    uint32_t Height() const noexcept { return height; }
    vk::RenderPass RenderPass() const noexcept { return renderPass; }
    vk::Buffer MatrixBuffer() const noexcept { return matrixBuffer; }
    vk::CommandPool TransferCommandPool() const noexcept { return transferCommandPool; }

    struct TransferChunk
    {
        vk::CommandBuffer cmdBuf;
        vk::Buffer freeBuffer;
        VmaAllocation freeAlloc{};
        std::vector<std::tuple<vk::Buffer, vk::DeviceSize>> releasedBuffers;
    };

    void EnqueueTransfer(const TransferChunk& cmdBuf);
    void Render();
    void OnWindowSizeChanged() noexcept;

    vk::ShaderModule LoadShader(const std::filesystem::path& path);
    const vk::SurfaceFormatKHR surfaceFormat;

private:
    std::vector<vk::CommandBuffer> graphicsCmdBuffers;
    std::vector<vk::CommandBuffer> acquireImageForPresentCmdBuffers;
    std::vector<vk::Fence> fences;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::Framebuffer> framebuffers;
    std::vector<vk::ImageView> swapchainImageViews;
    std::vector<vk::CommandBuffer> transferBuffers;
    std::vector<std::tuple<vk::Buffer, VmaAllocation>> transferGarbage;
    std::vector<std::tuple<vk::Buffer, vk::DeviceSize>> releasedBuffers;

    vk::SwapchainKHR swapchain;
    vk::Fence transferCompleted;
    vk::Semaphore renderingFinished;
    vk::Semaphore imageReady;
    vk::Semaphore imageAcquiredForPresent;
    vk::CommandPool graphicsCommandPool;
    vk::CommandPool presentCommandPool;
    vk::CommandPool transferCommandPool;
    vk::RenderPass renderPass;
    std::unique_ptr<OriginGizmo> originGizmo;
    vk::Buffer matrixBuffer;
    VmaAllocation matrixBufferAlloc{};
    VmaAllocationInfo matrixBufferAllocInfo{};

    vk::PresentModeKHR presentMode;
    uint32_t width{};
    uint32_t height{};
    bool recreateSwapchain{};

    vk::SurfaceFormatKHR DetermineSurfaceFormat();
    bool CreateSwapchain();
};
