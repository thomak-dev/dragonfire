#include "pch.h"

#include "Graphics.h"

#include "Engine.h"
#include "OriginGizmo.h"
#include "dfmath.h"

namespace gfx
{

constexpr std::array presentModePrio = {vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox};

Graphics::Graphics(SDL_Window* window, std::string_view title)
    : GraphicsBase{window, title}, surfaceFormat{DetermineSurfaceFormat()}, matrices{glm::mat4(1), glm::mat4(1)}
{
    presentCommandPool = device.createCommandPool(vk::CommandPoolCreateInfo{}.setQueueFamilyIndex(presentQueueFamily));
    graphicsCommandPool =
        device.createCommandPool(vk::CommandPoolCreateInfo{}
                                     .setQueueFamilyIndex(graphicsQueueFamily)
                                     .setFlags(vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer));
    transferCommandPool =
        device.createCommandPool(vk::CommandPoolCreateInfo{}.setQueueFamilyIndex(transferQueueFamily).setFlags(vk::CommandPoolCreateFlagBits::eTransient));

    auto surfacePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    std::sort(surfacePresentModes.begin(), surfacePresentModes.end(), [](auto a, auto b) {
        return std::distance(presentModePrio.begin(), std::find(presentModePrio.begin(), presentModePrio.end(), a)) <
               std::distance(presentModePrio.begin(), std::find(presentModePrio.begin(), presentModePrio.end(), b));
    });
    presentMode = surfacePresentModes.front();

    const auto attachment = vk::AttachmentDescription{}
                                .setFormat(surfaceFormat.format)
                                .setLoadOp(vk::AttachmentLoadOp::eClear)
                                .setInitialLayout(vk::ImageLayout::eUndefined)
                                .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                .setSamples(vk::SampleCountFlagBits::e1);
    const auto attachRef = vk::AttachmentReference{}.setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
    const auto subpass = vk::SubpassDescription{}.setColorAttachments(attachRef).setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    renderPass = device.createRenderPass(vk::RenderPassCreateInfo{}.setAttachments(attachment).setSubpasses(subpass));

    CreateSwapchain();

    renderingFinished = device.createSemaphore({});
    imageReady = device.createSemaphore({});
    imageAcquiredForPresent = device.createSemaphore({});
    transferCompleted = device.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));

    Resources().SetLoaderAndDestroyer<vk::ShaderModule>([this](auto path) { return static_cast<VkShaderModule>(LoadShader(path)); },
                                                        [this](void* sm) noexcept { device.destroyShaderModule(reinterpret_cast<VkShaderModule>(sm)); });

    const VkBufferCreateInfo& ubCreateInfo = vk::BufferCreateInfo{}.setUsage(vk::BufferUsageFlagBits::eUniformBuffer).setSize(sizeof(MatrixBlock));
    VmaAllocationCreateInfo ubAllocInfo{};
    ubAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    ubAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    if (const auto result =
            vmaCreateBuffer(allocator, &ubCreateInfo, &ubAllocInfo, reinterpret_cast<VkBuffer*>(&matrixBuffer), &matrixBufferAlloc, &matrixBufferAllocInfo))
        throw std::runtime_error{vk::to_string(static_cast<vk::Result>(result))};

    std::memcpy(matrixBufferAllocInfo.pMappedData, &matrices, sizeof(MatrixBlock));
    vmaFlushAllocation(allocator, matrixBufferAlloc, 0, VK_WHOLE_SIZE);

    originGizmo = std::make_unique<OriginGizmo>();
}

Graphics::~Graphics()
{
    try
    {
        device.waitIdle();
    }
    catch (const vk::SystemError& err)
    {
        try
        {
            std::cout << "~Graphics(): " << err.what() << std::endl;
        }
        catch (...)
        {
        }
    }
    vmaDestroyBuffer(allocator, matrixBuffer, matrixBufferAlloc);
    for (size_t i = 0; i < fences.size(); ++i)
    {
        device.destroyFence(fences[i]);
    }
    device.destroyFence(transferCompleted);
    for (size_t i = 0; i < swapchainImageViews.size(); ++i)
    {
        device.destroyFramebuffer(framebuffers[i]);
        device.destroyImageView(swapchainImageViews[i]);
    }
    device.destroyRenderPass(renderPass);
    device.destroyCommandPool(graphicsCommandPool);
    device.destroyCommandPool(presentCommandPool);
    device.destroyCommandPool(transferCommandPool);
    device.destroySemaphore(renderingFinished);
    device.destroySemaphore(imageReady);
    device.destroySemaphore(imageAcquiredForPresent);
    device.destroySwapchainKHR(swapchain);
}

vk::SurfaceFormatKHR Graphics::DetermineSurfaceFormat()
{
    auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
    constexpr std::array ColorSpacePrio = {vk::ColorSpaceKHR::eSrgbNonlinear};
    constexpr std::array FormatPrio = {vk::Format::eR8G8B8A8Srgb, vk::Format::eB8G8R8A8Srgb, vk::Format::eA8B8G8R8SrgbPack32};
    std::stable_sort(surfaceFormats.begin(), surfaceFormats.end(), [&](const auto& a, const auto& b) {
        return std::distance(ColorSpacePrio.begin(), std::find(ColorSpacePrio.begin(), ColorSpacePrio.end(), a.colorSpace)) <
               std::distance(ColorSpacePrio.begin(), std::find(ColorSpacePrio.begin(), ColorSpacePrio.end(), b.colorSpace));
    });
    std::stable_sort(surfaceFormats.begin(), surfaceFormats.end(), [&](const auto& a, const auto& b) {
        return std::distance(FormatPrio.begin(), std::find(FormatPrio.begin(), FormatPrio.end(), a.format)) <
               std::distance(FormatPrio.begin(), std::find(FormatPrio.begin(), FormatPrio.end(), b.format));
    });
    return surfaceFormats.front();
}

bool Graphics::CreateSwapchain()
{
    auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    width = surfaceCaps.currentExtent.width;
    height = surfaceCaps.currentExtent.height;

    if (width > 0 && height > 0)
    {
        std::cout << "SurfaceCapabilities.currentExtent: " << surfaceCaps.currentExtent.width << 'x' << surfaceCaps.currentExtent.height << '\n';

        const auto oldSwapchain = swapchain;
        const auto swapchainInfo =
            vk::SwapchainCreateInfoKHR{}
                .setClipped(true)
                .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                .setImageArrayLayers(1)
                .setImageFormat(surfaceFormat.format)
                .setImageColorSpace(surfaceFormat.colorSpace)
                .setImageExtent(vk::Extent2D{std::clamp(width, surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width),
                                             std::clamp(height, surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height)})
                .setImageUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment)
                .setMinImageCount(std::max(surfaceCaps.minImageCount,
                                           (surfaceCaps.maxImageCount ? std::min(surfaceCaps.maxImageCount, swapchainImagesDesired) : swapchainImagesDesired)))
                .setPresentMode(presentMode)
                .setPreTransform(surfaceCaps.currentTransform)
                .setSurface(surface)
                .setOldSwapchain(oldSwapchain);

        std::cout << "Swapchain.minImageCount: " << swapchainInfo.minImageCount << '\n';
        swapchain = device.createSwapchainKHR(swapchainInfo);
        device.waitIdle();
        if (swapchainImages.size() > 0)
            for (size_t i = 0; i < swapchainImages.size(); ++i)
            {
                device.destroyFramebuffer(framebuffers[i]);
                device.destroyImageView(swapchainImageViews[i]);
            }
        device.destroySwapchainKHR(oldSwapchain);

        if (acquireImageForPresentCmdBuffers.size() > 0)
        {
            device.freeCommandBuffers(presentCommandPool, acquireImageForPresentCmdBuffers);
        }
        swapchainImages = device.getSwapchainImagesKHR(swapchain);
        framebuffers.resize(swapchainImages.size());
        swapchainImageViews.resize(swapchainImages.size());

        std::cout << "Swapchain image count: " << swapchainImages.size() << '\n';
        std::cout << "Desired image count: " << swapchainImagesDesired << std::endl;
        std::vector<vk::CommandBuffer> cmdBuffers = device.allocateCommandBuffers(
            vk::CommandBufferAllocateInfo{}.setCommandPool(presentCommandPool).setCommandBufferCount(static_cast<uint32_t>(swapchainImages.size())));

        acquireImageForPresentCmdBuffers.resize(swapchainImages.size());
        std::copy(cmdBuffers.begin(), cmdBuffers.begin() + swapchainImages.size(), acquireImageForPresentCmdBuffers.begin());
        if (fences.size() > swapchainImages.size())
        {
            std::for_each(fences.begin() + swapchainImages.size(), fences.end(), [&](auto& fence) noexcept { device.destroyFence(fence); });
            std::for_each(graphicsCmdBuffers.begin() + swapchainImages.size(), graphicsCmdBuffers.end(),
                          [&](auto& buf) noexcept { device.freeCommandBuffers(graphicsCommandPool, buf); });
        }
        fences.resize(swapchainImages.size());
        const auto graphicsCmdBuffersSizeBefore = graphicsCmdBuffers.size();
        graphicsCmdBuffers.resize(swapchainImages.size());
        for (size_t i = 0; i < swapchainImages.size(); ++i)
        {
            acquireImageForPresentCmdBuffers[i].begin(vk::CommandBufferBeginInfo{});
            acquireImageForPresentCmdBuffers[i].pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr,
                vk::ImageMemoryBarrier{}
                    .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                    .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                    .setImage(swapchainImages[i])
                    .setSrcQueueFamilyIndex(graphicsQueueFamily)
                    .setDstQueueFamilyIndex(presentQueueFamily)
                    .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1)));
            acquireImageForPresentCmdBuffers[i].end();
            if (!fences[i])
                fences[i] = device.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));

            swapchainImageViews[i] = device.createImageView(
                vk::ImageViewCreateInfo{}
                    .setFormat(surfaceFormat.format)
                    .setImage(swapchainImages[i])
                    .setViewType(vk::ImageViewType::e2D)
                    .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1)));
            framebuffers[i] = device.createFramebuffer(
                vk::FramebufferCreateInfo{}.setLayers(1).setRenderPass(renderPass).setWidth(width).setHeight(height).setAttachments(swapchainImageViews[i]));
        }
        if (graphicsCmdBuffersSizeBefore < graphicsCmdBuffers.size())
        {
            const auto count = graphicsCmdBuffers.size() - graphicsCmdBuffersSizeBefore;
            auto bufs = device.allocateCommandBuffers(
                vk::CommandBufferAllocateInfo{}.setCommandPool(graphicsCommandPool).setCommandBufferCount(static_cast<uint32_t>(count)));
            std::copy(bufs.begin(), bufs.end(), graphicsCmdBuffers.begin() + graphicsCmdBuffersSizeBefore);
        }

        return true;
    }
    return false;
}

void Graphics::Render()
{
    if (!transferBuffers.empty() && device.getFenceStatus(transferCompleted) == vk::Result::eSuccess)
    {
        device.resetFences(transferCompleted);
        transferQueue.submit(vk::SubmitInfo{}.setCommandBuffers(transferBuffers), transferCompleted);
        transferBuffers.clear();
    }

    if (recreateSwapchain)
    {
        const auto success = CreateSwapchain();
        if (success)
            recreateSwapchain = false;
        else
            return;
    }

    uint32_t image{};
    try
    {
        const auto acquireResult = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), imageReady);

        if (acquireResult.result == vk::Result::eSuboptimalKHR)
        {
            std::cout << "Acquire: Suboptimal" << std::endl;
            recreateSwapchain = true;
            return;
        }

        image = acquireResult.value;
        vmaSetCurrentFrameIndex(allocator, image);
    }
    catch (vk::OutOfDateKHRError&)
    {
        std::cout << "Acquire: Out of date" << std::endl;
        recreateSwapchain = true;
        return;
    }

    if (matricesDirty)
    {
        std::memcpy(matrixBufferAllocInfo.pMappedData, &matrices, sizeof(MatrixBlock));
        vmaFlushAllocation(allocator, matrixBufferAlloc, 0, VK_WHOLE_SIZE);
        matricesDirty = false;
    }

    std::ignore = device.waitForFences({fences[image], transferCompleted}, true, std::numeric_limits<uint64_t>::max());
    device.resetFences(fences[image]);
    auto& cmdBuffer = graphicsCmdBuffers[image];
    cmdBuffer.begin(vk::CommandBufferBeginInfo{}.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    for (size_t i = 0; i < releasedBuffers.size(); ++i)
    {
        cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput, {}, {},
                                  vk::BufferMemoryBarrier{}
                                      .setBuffer(std::get<0>(releasedBuffers[i]))
                                      .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                                      .setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead)
                                      .setSize(std::get<1>(releasedBuffers[i]))
                                      .setSrcQueueFamilyIndex(transferQueueFamily)
                                      .setDstQueueFamilyIndex(graphicsQueueFamily),
                                  {});
    }
    const auto clear = vk::ClearValue{}.setColor(vk::ClearColorValue{std::array{1.f, 0.f, 0.f, 0.f}});
    cmdBuffer.beginRenderPass(vk::RenderPassBeginInfo{}
                                  .setClearValues(clear)
                                  .setRenderArea(vk::Rect2D{}.setExtent({width, height}))
                                  .setRenderPass(renderPass)
                                  .setFramebuffer(framebuffers[image]),
                              {});
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, originGizmo->pipeline);
    cmdBuffer.setViewport(0, vk::Viewport{}.setMinDepth(0).setMaxDepth(1).setWidth(static_cast<float>(width)).setHeight(static_cast<float>(height)));
    cmdBuffer.setScissor(0, vk::Rect2D{}.setExtent({width, height}));
    cmdBuffer.bindIndexBuffer(originGizmo->indexBuffer, 0, vk::IndexType::eUint32);
    cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, originGizmo->pipelineLayout, 0, originGizmo->descriptorSet, nullptr);
    cmdBuffer.bindVertexBuffers(0, originGizmo->vertexBuffer, {0});
    cmdBuffer.drawIndexed(18, 1, 0, 0, 0);
    cmdBuffer.endRenderPass();
    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr,
        vk::ImageMemoryBarrier{}
            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setImage(swapchainImages[image])
            .setSrcQueueFamilyIndex(graphicsQueueFamily)
            .setDstQueueFamilyIndex(presentQueueFamily)
            .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1))
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite));
    cmdBuffer.end();
    const bool shouldAcquireImageForPresentQueue = graphicsQueueFamily != presentQueueFamily;
    vk::PipelineStageFlags waitDst = vk::PipelineStageFlagBits::eTransfer;
    graphicsQueue.submit(
        vk::SubmitInfo{}.setCommandBuffers(cmdBuffer).setSignalSemaphores(renderingFinished).setWaitSemaphores(imageReady).setWaitDstStageMask(waitDst),
        shouldAcquireImageForPresentQueue ? nullptr : fences[image]);

    if (shouldAcquireImageForPresentQueue)
    {
        waitDst = vk::PipelineStageFlagBits::eAllCommands; // according to khronos
        IgnoreVkMessage(0x48a09f6c);
        presentQueue.submit(vk::SubmitInfo{}
                                .setWaitSemaphores(renderingFinished)
                                .setWaitDstStageMask(waitDst)
                                .setCommandBuffers(acquireImageForPresentCmdBuffers[image])
                                .setSignalSemaphores(imageAcquiredForPresent),
                            fences[image]);
        UnignoreVkMessage(0x48a09f6c);
    }

    vk::Result presentResult{};
    try
    {
        presentResult = presentQueue.presentKHR(vk::PresentInfoKHR{}
                                                    .setImageIndices(image)
                                                    .setWaitSemaphores(shouldAcquireImageForPresentQueue ? imageAcquiredForPresent : renderingFinished)
                                                    .setSwapchains(swapchain));

        if (presentResult == vk::Result::eSuboptimalKHR)
        {
            std::cout << "Present: Suboptimal" << std::endl;
            recreateSwapchain = true;
        }
    }
    catch (vk::OutOfDateKHRError&)
    {
        std::cout << "Present: Out of date" << std::endl;
        recreateSwapchain = true;
    }

    for (size_t i = 0; i < transferGarbage.size(); ++i)
    {
        vmaDestroyBuffer(allocator, std::get<0>(transferGarbage[i]), std::get<1>(transferGarbage[i]));
    }
    transferGarbage.clear();
    releasedBuffers.clear();
}

void Graphics::OnWindowSizeChanged() noexcept
{
    recreateSwapchain = true;
}

void Graphics::ViewMatrix(const glm::mat4& view) noexcept
{
    matrices.view = view;
    matricesDirty = true;
}

void Graphics::ProjectionMatrix(const glm::mat4& projection) noexcept
{
    matrices.projection = projection;
    matricesDirty = true;
}

void Graphics::EnqueueTransfer(const TransferChunk& cmdBuf)
{
    transferBuffers.push_back(cmdBuf.cmdBuf);
    if (cmdBuf.freeAlloc)
        transferGarbage.emplace_back(cmdBuf.freeBuffer, cmdBuf.freeAlloc);

    for (size_t i = 0; i < cmdBuf.releasedBuffers.size(); ++i)
    {
        releasedBuffers.push_back(cmdBuf.releasedBuffers[i]);
    }
}

vk::ShaderModule Graphics::LoadShader(const std::filesystem::path& path)
{
    auto bytes = Engine::Instance().LoadBinaryFile(path);
    return device.createShaderModule(vk::ShaderModuleCreateInfo{}.setCodeSize(bytes.size()).setPCode(reinterpret_cast<uint32_t*>(bytes.data())));
}

} // namespace gfx