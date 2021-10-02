#include "Graphics.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

vk::Instance Graphics::instance{};
constexpr std::array presentModePrio = {vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox};
constexpr uint32_t swapchainImagesDesired = 3;
static std::map<uint32_t, int> ignoredMessages;

void IgnoreVkMessage(uint32_t messageId)
{
    ++ignoredMessages[messageId];
}

void UnignoreVkMessage(uint32_t messageId)
{
    --ignoredMessages[messageId];
}

VKAPI_ATTR VkBool32 VKAPI_PTR OnVkDebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    (void)pUserData;
    auto data = reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>(pCallbackData);
    if (vk::DebugUtilsMessageSeverityFlagBitsEXT(messageSeverity) > vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose && ignoredMessages[data->messageIdNumber] == 0)
    {
        std::cout << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
                  << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(messageTypes)) << ": ";
        if (data->pMessageIdName)
            std::cout << data->pMessageIdName;
        std::cout << ": " << data->pMessage << std::endl;
    }

    if (ignoredMessages[data->messageIdNumber] < 0)
        std::cout << "Warning: Imbalanced ignored messages." << std::endl;

    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        throw std::runtime_error{data->pMessage};

    return VK_FALSE;
}

bool CheckQueueFamilies(const vk::ArrayProxy<const vk::QueueFamilyProperties>& queueFamProps, vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    bool graphics{};
    bool transfer{};
    bool present{};
    for (uint32_t i = 0; i < queueFamProps.size(); ++i)
    {
        if (queueFamProps.data()[i].queueFlags & vk::QueueFlagBits::eGraphics)
            graphics = true;
        if (queueFamProps.data()[i].queueFlags & vk::QueueFlagBits::eTransfer)
            transfer = true;
        if (device.getSurfaceSupportKHR(i, surface))
            present = true;

        if (graphics && transfer && present)
            break;
    }
    return graphics && transfer && present;
}

Graphics::Graphics(SDL_Window* window, std::string_view title, std::string_view engineName)
{
    uint32_t numExtensions;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, nullptr))
        throw std::runtime_error{SDL_GetError()};
    std::vector<const char*> instExtensions(numExtensions);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, instExtensions.data()))
        throw std::runtime_error{SDL_GetError()};

    auto instExtProps = vk::enumerateInstanceExtensionProperties();
    std::cout << "Vulkan instance extensions:\n";
    for (auto& prop : instExtProps)
        std::cout << '\t' << prop.extensionName << std::endl;

    void* validationFeatures = nullptr;
    std::vector<const char*> layers;
#ifndef NDEBUG
    std::cout << "Vulkan instance layer properties:\n";
    auto layerProps = vk::enumerateInstanceLayerProperties();
    for (auto& prop : layerProps)
        std::cout << '\t' << prop.layerName << ": " << prop.description << std::endl;

    layers.push_back("VK_LAYER_KHRONOS_validation");
    layers.push_back("VK_LAYER_LUNARG_monitor");
    std::array enables = {vk::ValidationFeatureEnableEXT::eBestPractices, vk::ValidationFeatureEnableEXT::eSynchronizationValidation};
    auto features = vk::ValidationFeaturesEXT{}.setEnabledValidationFeatures(enables);
    validationFeatures = &features;

    instExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    auto dbgCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{}
                             .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                                 vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
                             .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                             vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
                             .setPfnUserCallback(OnVkDebugUtilsMessengerCallback);
    features.setPNext(&dbgCreateInfo);
#endif

    auto appInfo = vk::ApplicationInfo{}
                       .setPApplicationName(title.data())
                       .setApplicationVersion(1)
                       .setPEngineName(engineName.data())
                       .setEngineVersion(1)
                       .setApiVersion(VK_API_VERSION_1_0);

    auto instInfo = vk::InstanceCreateInfo{}
                        .setFlags(vk::InstanceCreateFlags())
                        .setPApplicationInfo(&appInfo)
                        .setEnabledExtensionCount(static_cast<uint32_t>(instExtensions.size()))
                        .setPpEnabledExtensionNames(instExtensions.data())
                        .setEnabledLayerCount(static_cast<uint32_t>(layers.size()))
                        .setPpEnabledLayerNames(layers.data())
                        .setPNext(validationFeatures);

    instance = vk::createInstance(instInfo);

#ifndef NDEBUG
    dbgMessenger = instance.createDebugUtilsMessengerEXT(dbgCreateInfo);
#endif

    VkSurfaceKHR cSurface;

    if (!SDL_Vulkan_CreateSurface(window, instance, &cSurface))
        throw std::runtime_error{SDL_GetError()};
    surface = cSurface;
    auto physicalDevices = instance.enumeratePhysicalDevices();

    for (size_t i = 0; i < physicalDevices.size(); ++i)
    {
        std::cout << "Device[" << i << "] extensions:\n";
        for (auto& ext : physicalDevices[i].enumerateDeviceExtensionProperties())
            std::cout << '\t' << ext.extensionName << std::endl;
    }

    std::array deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::sort(deviceExtensions.begin(), deviceExtensions.end());
    const auto extCheck = [&](const auto& a) {
        std::vector<const char*> extNames;
        std::transform(a.begin(), a.end(), std::back_inserter(extNames), [](const auto& element) { return element.extensionName; });
        return std::includes(extNames.begin(), extNames.end(), deviceExtensions.begin(), deviceExtensions.end());
    };
    std::sort(physicalDevices.begin(), physicalDevices.end(), [=](auto a, auto b) {
        return a.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu && b.getProperties().deviceType != vk::PhysicalDeviceType::eDiscreteGpu ||
               CheckQueueFamilies(a.getQueueFamilyProperties(), a, surface) && !CheckQueueFamilies(b.getQueueFamilyProperties(), b, surface) ||
               extCheck(a.enumerateDeviceExtensionProperties()) && !extCheck(b.enumerateDeviceExtensionProperties()) ||
               a.getProperties().apiVersion > b.getProperties().apiVersion;
    });

    physicalDevice = physicalDevices.front();
    auto queueFams = physicalDevice.getQueueFamilyProperties();
    AssignQueueFamiliyIndices(queueFams);

    std::map<uint32_t, uint32_t> indicesMap;
    indicesMap[transferQueueFamily]++;
    indicesMap[graphicsQueueFamily]++;
    indicesMap[presentQueueFamily]++;

    std::vector<float> prios;
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos(indicesMap.size());
    size_t i = 0;
    for (auto& it : indicesMap)
    {
        auto actualCount = std::min(queueFams[it.first].queueCount, it.second);
        it.second = actualCount;
        if (prios.size() < actualCount)
        {
            auto end_before = prios.size();
            prios.resize(actualCount);
            std::fill(prios.begin() + end_before, prios.end(), 1.0f);
        }
        queueCreateInfos[i].setPQueuePriorities(prios.data()).setQueueFamilyIndex(it.first).setQueueCount(actualCount);
        ++i;
    }

    device = physicalDevice.createDevice(
        vk::DeviceCreateInfo{}.setQueueCreateInfos(queueCreateInfos).setPEnabledLayerNames(layers).setPEnabledExtensionNames(deviceExtensions));
    auto idx = indicesMap[graphicsQueueFamily];
    if (idx > 0)
        indicesMap[graphicsQueueFamily] = idx - 1;
    graphicsQueue = device.getQueue(graphicsQueueFamily, idx - 1);

    idx = indicesMap[transferQueueFamily];
    if (idx > 0)
        indicesMap[transferQueueFamily] = idx - 1;
    transferQueue = device.getQueue(transferQueueFamily, idx - 1);

    idx = indicesMap[presentQueueFamily];
    if (idx > 0)
        indicesMap[presentQueueFamily] = idx - 1;
    presentQueue = device.getQueue(presentQueueFamily, idx - 1);

    presentCommandPool = device.createCommandPool(vk::CommandPoolCreateInfo{}.setQueueFamilyIndex(presentQueueFamily));
    graphicsCommandPool =
        device.createCommandPool(vk::CommandPoolCreateInfo{}
                                     .setQueueFamilyIndex(graphicsQueueFamily)
                                     .setFlags(vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer));

    auto surfacePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    std::sort(surfacePresentModes.begin(), surfacePresentModes.end(), [](auto a, auto b) {
        return std::distance(presentModePrio.begin(), std::find(presentModePrio.begin(), presentModePrio.end(), a)) <
               std::distance(presentModePrio.begin(), std::find(presentModePrio.begin(), presentModePrio.end(), b));
    });
    presentMode = surfacePresentModes.front();

    DetermineSurfaceFormat();
    CreateSwapchain();

    renderingFinished = device.createSemaphore({});
    imageReady = device.createSemaphore({});
    imageAcquiredForPresent = device.createSemaphore({});
}

Graphics::~Graphics()
{
    device.waitIdle();
    for (size_t i = 0; i < fences.size(); ++i)
    {
        device.destroyFence(fences[i]);
    }
    device.destroyCommandPool(graphicsCommandPool);
    device.destroyCommandPool(presentCommandPool);
    device.destroySemaphore(renderingFinished);
    device.destroySemaphore(imageReady);
    device.destroySemaphore(imageAcquiredForPresent);
    device.destroySwapchainKHR(swapchain);
    device.destroy();
    instance.destroySurfaceKHR(surface);
#ifndef NDEBUG
    instance.destroyDebugUtilsMessengerEXT(dbgMessenger);
#endif
    instance.destroy();
}

void Graphics::AssignQueueFamiliyIndices(const std::vector<vk::QueueFamilyProperties>& queueFams)
{
    std::vector<std::set<uint32_t>> indexSets(3);
    std::vector targetIndices = {&presentQueueFamily, &graphicsQueueFamily, &transferQueueFamily};
    for (uint32_t i = 0; i < queueFams.size(); ++i)
    {
        if (physicalDevice.getSurfaceSupportKHR(i, surface))
            indexSets[0].insert(i);
        if (queueFams[i].queueFlags & vk::QueueFlagBits::eGraphics)
            indexSets[1].insert(i);
        if (queueFams[i].queueFlags & vk::QueueFlagBits::eTransfer)
            indexSets[2].insert(i);
    }

    if (std::any_of(indexSets.begin(), indexSets.end(), [](const auto& set) { return set.empty(); }))
        throw std::runtime_error{"No suitable graphics device found."};

    std::vector<uint32_t> setIndicesToRemove;
    do
    {
        setIndicesToRemove.clear();
        for (uint32_t i = 0; i < indexSets.size(); ++i)
        {
            auto foundIndex = std::find_if(indexSets[i].begin(), indexSets[i].end(), [&](auto element) {
                for (size_t j = 0; j < indexSets.size() - 1; ++j)
                    if (indexSets[(i + j + 1) % indexSets.size()].count(element))
                        return false;
                return true;
            });
            if (foundIndex != indexSets[i].end())
            {
                *targetIndices[i] = *foundIndex;
                setIndicesToRemove.push_back(i);
            }
        }
        for (auto i : setIndicesToRemove)
        {
            indexSets.erase(indexSets.begin() + i);
            targetIndices.erase(targetIndices.begin() + i);
        }
    } while (setIndicesToRemove.size() > 0);

    for (size_t i = 0; i < indexSets.size(); ++i)
    {
        *targetIndices[i] = *indexSets[i].begin();
    }

    // if we got a weird transfer queue, use graphics queue for transfer
    if (queueFams[transferQueueFamily].minImageTransferGranularity != vk::Extent3D{1, 1, 1})
        transferQueueFamily = graphicsQueueFamily;
}

void Graphics::DetermineSurfaceFormat()
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
    surfaceFormat = surfaceFormats.front();
}

bool Graphics::CreateSwapchain()
{
    auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    uint32_t width = surfaceCaps.currentExtent.width, height = surfaceCaps.currentExtent.height;

    if (width > 0 && height > 0)
    {
        std::cout << "SurfaceCapabilities.currentExtent: " << surfaceCaps.currentExtent.width << 'x' << surfaceCaps.currentExtent.height << '\n';

        auto oldSwapchain = swapchain;
        auto swapchainInfo =
            vk::SwapchainCreateInfoKHR{}
                .setClipped(true)
                .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                .setImageArrayLayers(1)
                .setImageFormat(surfaceFormat.format)
                .setImageColorSpace(surfaceFormat.colorSpace)
                .setImageExtent(vk::Extent2D{std::clamp(width, surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width),
                                             std::clamp(height, surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height)})
                .setImageUsage(vk::ImageUsageFlagBits::eTransferDst)
                .setMinImageCount(std::max(surfaceCaps.minImageCount,
                                           (surfaceCaps.maxImageCount ? std::min(surfaceCaps.maxImageCount, swapchainImagesDesired) : swapchainImagesDesired)))
                .setPresentMode(presentMode)
                .setPreTransform(surfaceCaps.currentTransform)
                .setSurface(surface)
                .setOldSwapchain(oldSwapchain);

        std::cout << "Swapchain.minImageCount: " << swapchainInfo.minImageCount << '\n';
        swapchain = device.createSwapchainKHR(swapchainInfo);
        device.waitIdle();
        device.destroySwapchainKHR(oldSwapchain);

        if (acquireImageForPresentCmdBuffers.size() > 0)
        {
            device.freeCommandBuffers(presentCommandPool, acquireImageForPresentCmdBuffers);
        }
        swapchainImages = device.getSwapchainImagesKHR(swapchain);
        std::cout << "Swapchain image count: " << swapchainImages.size() << '\n';
        std::cout << "Desired image count: " << swapchainImagesDesired << std::endl;
        std::vector<vk::CommandBuffer> cmdBuffers = device.allocateCommandBuffers(
            vk::CommandBufferAllocateInfo{}.setCommandPool(presentCommandPool).setCommandBufferCount((uint32_t)swapchainImages.size()));

        acquireImageForPresentCmdBuffers.resize(swapchainImages.size());
        std::copy(cmdBuffers.begin(), cmdBuffers.begin() + swapchainImages.size(), acquireImageForPresentCmdBuffers.begin());
        if (fences.size() > swapchainImages.size())
        {
            std::for_each(fences.begin() + swapchainImages.size(), fences.end(), [&](auto& fence) { device.destroyFence(fence); });
            std::for_each(graphicsCmdBuffers.begin() + swapchainImages.size(), graphicsCmdBuffers.end(),
                          [&](auto& buf) { device.freeCommandBuffers(graphicsCommandPool, buf); });
        }
        fences.resize(swapchainImages.size());
        auto graphicsCmdBuffersSizeBefore = graphicsCmdBuffers.size();
        graphicsCmdBuffers.resize(swapchainImages.size());
        for (size_t i = 0; i < swapchainImages.size(); ++i)
        {
            acquireImageForPresentCmdBuffers[i].begin(vk::CommandBufferBeginInfo{});
            acquireImageForPresentCmdBuffers[i].pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr,
                vk::ImageMemoryBarrier{}
                    .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                    .setImage(swapchainImages[i])
                    .setSrcQueueFamilyIndex(graphicsQueueFamily)
                    .setDstQueueFamilyIndex(presentQueueFamily)
                    .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1)));
            acquireImageForPresentCmdBuffers[i].end();

            if (!fences[i])
                fences[i] = device.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
        }
        if (graphicsCmdBuffersSizeBefore < graphicsCmdBuffers.size())
        {
            auto count = graphicsCmdBuffers.size() - graphicsCmdBuffersSizeBefore;
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
    if (recreateSwapchain)
    {
        auto success = CreateSwapchain();
        if (success)
            recreateSwapchain = false;
        else
            return;
    }

    uint32_t image;
    try
    {
        auto acquireResult = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), imageReady);

        if (acquireResult.result == vk::Result::eSuboptimalKHR)
        {
            std::cout << "Acquire: Suboptimal" << std::endl;
            recreateSwapchain = true;
            return;
        }

        image = acquireResult.value;
    }
    catch (vk::OutOfDateKHRError&)
    {
        std::cout << "Acquire: Out of date" << std::endl;
        recreateSwapchain = true;
        return;
    }

    (void)device.waitForFences(fences[image], true, std::numeric_limits<uint64_t>::max());
    device.resetFences(fences[image]);
    auto& cmdBuffer = graphicsCmdBuffers[image];
    cmdBuffer.begin(vk::CommandBufferBeginInfo{}.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr,
        vk::ImageMemoryBarrier{}
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setImage(swapchainImages[image])
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1))
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite));
    cmdBuffer.clearColorImage(swapchainImages[image], vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{std::array{1.f, 0.f, 1.f, 1.f}},
                              vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1));
    cmdBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr,
        vk::ImageMemoryBarrier{}
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setImage(swapchainImages[image])
            .setSrcQueueFamilyIndex(graphicsQueueFamily)
            .setDstQueueFamilyIndex(presentQueueFamily)
            .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1))
            .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite));
    cmdBuffer.end();
    bool shouldAcquireImageForPresentQueue = graphicsQueueFamily != presentQueueFamily;
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

    vk::Result presentResult;
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
}

void Graphics::OnWindowSizeChanged()
{
    recreateSwapchain = true;
}
