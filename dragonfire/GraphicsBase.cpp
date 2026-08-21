#include "pch.h"

#include "GraphicsBase.h"

static std::map<uint32_t, int> ignoredMessages;

VKAPI_ATTR vk::Bool32 VKAPI_PTR OnVkDebugUtilsMessengerCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                                vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
                                                                const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::ignore = pUserData;
    auto data = pCallbackData;
    if (messageSeverity > vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose && ignoredMessages[data->messageIdNumber] == 0)
    {
        std::cout << vk::to_string(messageSeverity) << ": " << vk::to_string(messageTypes) << ": ";
        if (data->pMessageIdName)
            std::cout << data->pMessageIdName;
        std::cout << ": " << data->pMessage << std::endl;
    }

    if (ignoredMessages[data->messageIdNumber] < 0)
        std::cout << "Warning: Imbalanced ignored messages." << std::endl;

    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
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

GraphicsBase::GraphicsBase(SDL_Window* window, std::string_view title)
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

    const auto dbgCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{}
                                   .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
                                   .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
                                   .setPfnUserCallback(OnVkDebugUtilsMessengerCallback);
    features.setPNext(&dbgCreateInfo);
#endif

    // Vulkan wants null terminated strings, which a string_view does not promise. This has to
    // outlive appInfo, which only borrows the pointer, and with it the createInstance below.
    const std::string appName{title};
    const auto appInfo = vk::ApplicationInfo{}
                             .setPApplicationName(appName.c_str())
                             .setApplicationVersion(1)
                             .setPEngineName(appName.c_str())
                             .setEngineVersion(1)
                             .setApiVersion(VK_API_VERSION_1_0);

    const auto instInfo = vk::InstanceCreateInfo{}
                              .setFlags(vk::InstanceCreateFlags())
                              .setPApplicationInfo(&appInfo)
                              .setEnabledExtensionCount(static_cast<uint32_t>(instExtensions.size()))
                              .setPpEnabledExtensionNames(instExtensions.data())
                              .setEnabledLayerCount(static_cast<uint32_t>(layers.size()))
                              .setPpEnabledLayerNames(layers.data())
                              .setPNext(validationFeatures);

    vkInstance = vk::createInstance(instInfo);

#ifndef NDEBUG
    dbgMessenger = vkInstance.createDebugUtilsMessengerEXT(dbgCreateInfo);
#endif
    VkSurfaceKHR cSurface;

    if (!SDL_Vulkan_CreateSurface(window, vkInstance, &cSurface))
        throw std::runtime_error{SDL_GetError()};
    surface = cSurface;
    auto physicalDevices = vkInstance.enumeratePhysicalDevices();

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
            const auto end_before = prios.size();
            prios.resize(actualCount);
            std::fill(prios.begin() + end_before, prios.end(), 1.0f);
        }
        queueCreateInfos[i].setPQueuePriorities(prios.data()).setQueueFamilyIndex(it.first).setQueueCount(actualCount);
        ++i;
    }

    device = physicalDevice.createDevice(vk::DeviceCreateInfo{}.setQueueCreateInfos(queueCreateInfos).setPEnabledExtensionNames(deviceExtensions));

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

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT; // single threaded for now
    allocatorCreateInfo.instance = vkInstance;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;

    if (const auto result = vmaCreateAllocator(&allocatorCreateInfo, &allocator))
        throw std::runtime_error(vk::to_string(static_cast<vk::Result>(result)));
}

GraphicsBase::~GraphicsBase()
{
    vmaDestroyAllocator(allocator);
    device.destroy();
    vkInstance.destroySurfaceKHR(surface);
#ifndef NDEBUG
    vkInstance.destroyDebugUtilsMessengerEXT(dbgMessenger);
#endif
    vkInstance.destroy();
}

void GraphicsBase::IgnoreVkMessage(uint32_t messageId) noexcept
{
    std::ignore = messageId; // only used in debug builds
#ifndef NDEBUG
#pragma warning(suppress : 26447) // the map only throws when an allocation fails
    ++ignoredMessages[messageId];
#endif
}

void GraphicsBase::UnignoreVkMessage(uint32_t messageId) noexcept
{
    std::ignore = messageId; // only used in debug builds
#ifndef NDEBUG
#pragma warning(suppress : 26447) // the map only throws when an allocation fails
    --ignoredMessages[messageId];
#endif
}

void GraphicsBase::AssignQueueFamiliyIndices(const std::vector<vk::QueueFamilyProperties>& queueFams)
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

    if (std::any_of(indexSets.begin(), indexSets.end(), [](const auto& set) noexcept { return set.empty(); }))
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