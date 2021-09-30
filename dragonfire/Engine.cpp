#include "Engine.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <map>
#include <ranges>
#include <set>
#include <stdexcept>
#include <vector>

#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

static const char* EngineName = "Dragonfire Engine";
constexpr uint32_t additionalImages = 1;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                              const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    return func ? func(instance, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func)
        func(instance, messenger, pAllocator);
}

VKAPI_ATTR VkBool32 VKAPI_PTR OnVkDebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    auto data = reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>(pCallbackData);
    if (vk::DebugUtilsMessageSeverityFlagBitsEXT(messageSeverity) > vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
    {
        std::cout << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
                  << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(messageTypes)) << ": ";
        std::cout << data->pMessageIdName << ": " << data->pMessage << std::endl;
    }

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

Engine::Engine(std::string_view title)
{
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(title.data(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
        throw std::runtime_error{SDL_GetError()};

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
    using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MsgType = vk::DebugUtilsMessageTypeFlagBitsEXT;
    auto dbgCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{}
                             .setMessageSeverity(Severity::eError | Severity::eInfo | Severity::eVerbose | Severity::eWarning)
                             .setMessageType(MsgType::eGeneral | MsgType::ePerformance | MsgType::eValidation)
                             .setPfnUserCallback(OnVkDebugUtilsMessengerCallback);
    features.setPNext(&dbgCreateInfo);
#endif // !NDEBUG

    // vk::ApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    auto appInfo = vk::ApplicationInfo{}
                       .setPApplicationName(title.data())
                       .setApplicationVersion(1)
                       .setPEngineName(EngineName)
                       .setEngineVersion(1)
                       .setApiVersion(VK_API_VERSION_1_0);

    // vk::InstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    auto instInfo = vk::InstanceCreateInfo{}
                        .setFlags(vk::InstanceCreateFlags())
                        .setPApplicationInfo(&appInfo)
                        .setEnabledExtensionCount(static_cast<uint32_t>(instExtensions.size()))
                        .setPpEnabledExtensionNames(instExtensions.data())
                        .setEnabledLayerCount(static_cast<uint32_t>(layers.size()))
                        .setPpEnabledLayerNames(layers.data())
                        .setPNext(validationFeatures);

    // Create the Vulkan instance.
    instance = vk::createInstance(instInfo);
#ifndef NDEBUG
    dbgMessenger = instance.createDebugUtilsMessengerEXT(dbgCreateInfo);
#endif // !NDEBUG

    VkSurfaceKHR cSurface;
    // Create a Vulkan surface for rendering
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
    std::ranges::sort(deviceExtensions);
    const auto extCheck = [&](auto a) {
        std::ranges::sort(a);
        return std::ranges::includes(a, deviceExtensions, {}, [](auto& ax) { return std::string_view{ax.extensionName}; });
    };
    std::ranges::sort(physicalDevices, [=](auto a, auto b) {
        return a.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu && b.getProperties().deviceType != vk::PhysicalDeviceType::eDiscreteGpu ||
               CheckQueueFamilies(a.getQueueFamilyProperties(), a, surface) && !CheckQueueFamilies(b.getQueueFamilyProperties(), b, surface) ||
               extCheck(a.enumerateDeviceExtensionProperties()) && !extCheck(b.enumerateDeviceExtensionProperties()) ||
               a.getProperties().apiVersion > b.getProperties().apiVersion;
    });

    physicalDevice = physicalDevices.front();
    auto queueFams = physicalDevice.getQueueFamilyProperties();
    AssignQueueFamiliyIndices(queueFams);

    std::map<uint32_t, uint32_t> indicesMap;
    indicesMap[indexTransfer]++;
    indicesMap[indexGraphics]++;
    indicesMap[indexPresent]++;

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
    auto idx = indicesMap[indexGraphics];
    if (idx > 0)
        indicesMap[indexGraphics] = idx - 1;
    graphicsQueue = device.getQueue(indexGraphics, idx - 1);

    idx = indicesMap[indexTransfer];
    if (idx > 0)
        indicesMap[indexTransfer] = idx - 1;
    transferQueue = device.getQueue(indexTransfer, idx - 1);

    idx = indicesMap[indexPresent];
    if (idx > 0)
        indicesMap[indexPresent] = idx - 1;
    presentQueue = device.getQueue(indexPresent, idx - 1);

    auto surfacePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    constexpr std::array presentModePrio = {vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox};

    std::ranges::sort(surfacePresentModes, [](auto a, auto b) {
        return std::distance(presentModePrio.begin(), std::ranges::find(presentModePrio, a)) <
               std::distance(presentModePrio.begin(), std::ranges::find(presentModePrio, b));
    });
    presentMode = surfacePresentModes.front();

    DetermineSurfaceFormat();
    CreateSwapchain();

    renderingFinished = device.createSemaphore({});
    imageReady = device.createSemaphore({});
    for (size_t i = 0; i < 2; ++i)
    {
        graphicsCommandPools[i] = device.createCommandPool(vk::CommandPoolCreateInfo{}.setQueueFamilyIndex(indexGraphics));
        graphicsCmdBuffers[i] =
            device
                .allocateCommandBuffers(
                    vk::CommandBufferAllocateInfo{}.setCommandPool(graphicsCommandPools[i]).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1))
                .front();
        fences[i] = device.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
    }
}

void Engine::AssignQueueFamiliyIndices(const std::vector<vk::QueueFamilyProperties>& queueFams)
{
    std::vector<std::set<uint32_t>> indexSets(3);
    std::vector targetIndices = {&indexPresent, &indexGraphics, &indexTransfer};
    for (uint32_t i = 0; i < queueFams.size(); ++i)
    {
        if (queueFams[i].queueFlags & vk::QueueFlagBits::eGraphics)
            indexSets[1].insert(i);
        if (queueFams[i].queueFlags & vk::QueueFlagBits::eTransfer)
            indexSets[2].insert(i);
        if (physicalDevice.getSurfaceSupportKHR(i, surface))
            indexSets[0].insert(i);
    }

    if (std::ranges::any_of(indexSets, [](const auto& set) { return set.empty(); }))
        throw std::runtime_error{"No suitable graphics device found."};

    std::vector<uint32_t> setIndicesToRemove;
    do
    {
        setIndicesToRemove.clear();
        for (uint32_t i = 0; i < indexSets.size(); ++i)
        {
            auto foundIndex = std::ranges::find_if(indexSets[i], [&](auto element) {
                for (size_t j = 0; j < indexSets.size() - 1; ++j)
                    if (indexSets[(i + j + 1) % indexSets.size()].contains(element))
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
    if (queueFams[indexTransfer].minImageTransferGranularity != vk::Extent3D{1, 1, 1})
        indexTransfer = indexGraphics;
}

void Engine::DetermineSurfaceFormat()
{
    auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
    constexpr std::array ColorSpacePrio = {vk::ColorSpaceKHR::eSrgbNonlinear};
    constexpr std::array FormatPrio = {vk::Format::eR8G8B8A8Srgb, vk::Format::eB8G8R8A8Srgb, vk::Format::eA8B8G8R8SrgbPack32};
    std::ranges::stable_sort(surfaceFormats, [](const auto& a, const auto& b) {
        return std::distance(ColorSpacePrio.begin(), std::ranges::find(ColorSpacePrio, a.colorSpace)) <
               std::distance(ColorSpacePrio.begin(), std::ranges::find(ColorSpacePrio, b.colorSpace));
    });
    std::ranges::stable_sort(surfaceFormats, [](const auto& a, const auto& b) {
        return std::distance(FormatPrio.begin(), std::ranges::find(FormatPrio, a.format)) <
               std::distance(FormatPrio.begin(), std::ranges::find(FormatPrio, b.format));
    });
    surfaceFormat = surfaceFormats.front();
}

bool Engine::CreateSwapchain()
{
    auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    int w, h;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);

    uint32_t width = surfaceCaps.currentExtent.width, height = surfaceCaps.currentExtent.height;

    if (width > 0 && height > 0)
    {
        std::cout << "SurfaceCapabilities.currentExtent: " << surfaceCaps.currentExtent.width << 'x' << surfaceCaps.currentExtent.height << '\n';
        std::cout << "SDL_Vulkan_GetDrawableSize: " << w << 'x' << h << std::endl;
        auto concurrent = indexPresent != indexGraphics;
        std::array concurrentQueueFams = {indexPresent, indexGraphics};
        auto oldSwapchain = swapchain;
        auto swapchainInfo = vk::SwapchainCreateInfoKHR{}
                                 .setClipped(true)
                                 .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                                 .setImageArrayLayers(1)
                                 .setImageFormat(surfaceFormat.format)
                                 .setImageColorSpace(surfaceFormat.colorSpace)
                                 .setImageExtent(vk::Extent2D{std::clamp(width, surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width),
                                                              std::clamp(height, surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height)})
                                 .setImageSharingMode(concurrent ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
                                 .setImageUsage(vk::ImageUsageFlagBits::eTransferDst)
                                 .setMinImageCount(surfaceCaps.minImageCount +
                                                   (surfaceCaps.maxImageCount ? std::min(surfaceCaps.maxImageCount, additionalImages) : additionalImages))
                                 .setPresentMode(presentMode)
                                 .setPreTransform(surfaceCaps.currentTransform)
                                 .setSurface(surface)
                                 .setOldSwapchain(oldSwapchain);

        if (concurrent)
            swapchainInfo.setQueueFamilyIndices(concurrentQueueFams);

        swapchain = device.createSwapchainKHR(swapchainInfo);
        device.waitIdle();
        device.destroySwapchainKHR(oldSwapchain);
        swapchainImages = device.getSwapchainImagesKHR(swapchain);
        return true;
    }
    return false;
}

void Engine::Run()
{
    SDL_Event event;
    bool quit = false;
    int frame = -1;
    bool recreateSwapchain = false;
    while (!quit)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                quit = true;

            if (event.type == SDL_WINDOWEVENT)
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    std::cout << "Window size changed: " << event.window.data1 << 'x' << event.window.data2 << '\n';
                    recreateSwapchain = true;
                }
        }

        if (recreateSwapchain)
        {
            auto ok = CreateSwapchain();
            if (ok)
                recreateSwapchain = false;
        }

        if (recreateSwapchain)
            continue;

        frame = (frame + 1) % 2;
        uint32_t image;
        try
        {
            auto acquireResult = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageReady, nullptr);

            if (acquireResult.result == vk::Result::eSuboptimalKHR)
            {
                std::cout << "Acquire: Suboptimal" << std::endl;
                recreateSwapchain = true;
                continue;
            }

            image = acquireResult.value;
        }
        catch (vk::OutOfDateKHRError&)
        {
            std::cout << "Acquire: Out of date" << std::endl;
            recreateSwapchain = true;
            continue;
        }

        (void)device.waitForFences(fences[frame], true, UINT64_MAX);
        device.resetFences(fences[frame]);
        device.resetCommandPool(graphicsCommandPools[frame]);
        auto& cmdBuffer = graphicsCmdBuffers[frame];
        cmdBuffer.begin(vk::CommandBufferBeginInfo{}.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        cmdBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr,
            vk::ImageMemoryBarrier{}
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setImage(swapchainImages[image])
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1)));
        cmdBuffer.clearColorImage(swapchainImages[image], vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{},
                                  vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1));
        cmdBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {}, nullptr, nullptr,
            vk::ImageMemoryBarrier{}
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                .setImage(swapchainImages[image])
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setSubresourceRange(vk::ImageSubresourceRange{}.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(1))
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite));
        cmdBuffer.end();

        vk::PipelineStageFlags transferWaitDst = vk::PipelineStageFlagBits::eTransfer;
        graphicsQueue.submit(vk::SubmitInfo{}
                                 .setCommandBuffers(cmdBuffer)
                                 .setSignalSemaphores(renderingFinished)
                                 .setWaitSemaphores(imageReady)
                                 .setWaitDstStageMask(transferWaitDst),
                             fences[frame]);

        vk::Result presentResult;
        try
        {
            presentResult = presentQueue.presentKHR(vk::PresentInfoKHR{}.setImageIndices(image).setWaitSemaphores(renderingFinished).setSwapchains(swapchain));

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
}

Engine::~Engine()
{
    device.waitIdle();
    for (size_t i = 0; i < 2; ++i)
    {
        device.destroyFence(fences[i]);
        device.destroyCommandPool(graphicsCommandPools[i]);
    }
    device.destroySemaphore(renderingFinished);
    device.destroySemaphore(imageReady);
    device.destroySwapchainKHR(swapchain);
    device.destroy();
    instance.destroySurfaceKHR(surface);
    instance.destroyDebugUtilsMessengerEXT(dbgMessenger);
    instance.destroy();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
