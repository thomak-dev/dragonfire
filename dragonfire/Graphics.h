#pragma once

#include "GraphicsBase.h"

class OriginGizmo;

namespace gfx
{

class Graphics : public GraphicsBase
{
public:
    Graphics(SDL_Window* window, std::string_view title);
    Graphics() = delete;
    Graphics(Graphics&) = delete;
    Graphics(Graphics&&) = delete;
    Graphics& operator=(Graphics&) = delete;
    Graphics& operator=(Graphics&&) = delete;
    ~Graphics();

    // one uniform buffer and one descriptor set per frame slot,
    // so the CPU never writes a buffer a submission still in flight is reading
    static constexpr size_t MaxFramesInFlight = 2;

    uint32_t Width() const noexcept { return width; }
    uint32_t Height() const noexcept { return height; }
    vk::RenderPass RenderPass() const noexcept { return renderPass; }
    vk::Buffer MatrixBuffer(size_t frame) const noexcept { return matrixBuffers.at(frame); }
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
    void OnDpiChanged(uint32_t dpi) noexcept { this->dpi = dpi; };
    uint32_t Dpi() const noexcept { return dpi; }
    void ViewMatrix(const glm::mat4& view) noexcept;
    void ProjectionMatrix(const glm::mat4& projection) noexcept;
    const glm::mat4& ViewMatrix() const noexcept { return matrices.view; }
    const glm::mat4& ProjectionMatrix() const noexcept { return matrices.projection; }

    vk::ShaderModule LoadShader(const std::filesystem::path& path);
    const vk::SurfaceFormatKHR surfaceFormat;

private:
    struct MatrixBlock
    {
        glm::mat4 view;
        glm::mat4 projection;
    };

    std::vector<vk::CommandBuffer> graphicsCmdBuffers;
    std::vector<vk::CommandBuffer> acquireImageForPresentCmdBuffers;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::Framebuffer> framebuffers;
    std::vector<vk::ImageView> swapchainImageViews;
    std::vector<vk::CommandBuffer> transferBuffers;
    std::vector<std::tuple<vk::Buffer, VmaAllocation>> transferGarbage;
    std::vector<std::tuple<vk::Buffer, vk::DeviceSize>> releasedBuffers;

    vk::SwapchainKHR swapchain;
    vk::Fence transferCompleted;

    // one per swapchain image: present waits on these, so their lifetime is tied to the image
    std::vector<vk::Semaphore> renderingFinished;
    std::vector<vk::Semaphore> imageAcquiredForPresent;
    // borrowed handles into frameFences, recording which frame slot last used each image
    std::vector<vk::Fence> imagesInFlight;
    std::array<vk::Semaphore, MaxFramesInFlight> imageReady{};
    std::array<vk::Fence, MaxFramesInFlight> frameFences{};
    size_t frameIndex{};
    vk::CommandPool graphicsCommandPool;
    vk::CommandPool presentCommandPool;
    vk::CommandPool transferCommandPool;
    vk::RenderPass renderPass;
    std::unique_ptr<OriginGizmo> originGizmo;
    std::array<vk::Buffer, MaxFramesInFlight> matrixBuffers{};
    std::array<VmaAllocation, MaxFramesInFlight> matrixBufferAllocs{};
    std::array<VmaAllocationInfo, MaxFramesInFlight> matrixBufferAllocInfos{};

    vk::PresentModeKHR presentMode;
    MatrixBlock matrices;
    uint32_t width{};
    uint32_t height{};
    uint32_t dpi{};
    bool recreateSwapchain{};
    // per frame slot: a matrix change has to reach every slot's buffer, one slot at a time
    std::array<bool, MaxFramesInFlight> matricesDirty{};

    vk::SurfaceFormatKHR DetermineSurfaceFormat();
    bool CreateSwapchain();
};

}