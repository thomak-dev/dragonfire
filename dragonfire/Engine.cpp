#include "Engine.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

using namespace std::chrono_literals;

static const char* EngineName = "Dragonfire Engine";

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
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

VKAPI_ATTR VkBool32 VKAPI_PTR OnVkDebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cout << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": " << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(messageTypes)) << ": ";
    auto data = reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>(pCallbackData);
    std::cout << data->pMessageIdName << ": " << data->pMessage << std::endl;

    return VK_FALSE;
}

Engine::Engine(std::string_view title)
{
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(title.data(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
        throw std::runtime_error(SDL_GetError());

    unsigned numExtensions;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, nullptr))
        throw std::runtime_error{SDL_GetError()};
    std::vector<const char*> extensions(numExtensions);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, extensions.data()))
        throw std::runtime_error(SDL_GetError());

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
    VkValidationFeatureEnableEXT enables[] = {VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};
    VkValidationFeaturesEXT features = {};
    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.enabledValidationFeatureCount = 2;
    features.pEnabledValidationFeatures = enables;
    validationFeatures = &features;

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif // !NDEBUG

    // vk::ApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    vk::ApplicationInfo appInfo = vk::ApplicationInfo()
        .setPApplicationName(title.data())
        .setApplicationVersion(1)
        .setPEngineName(EngineName)
        .setEngineVersion(1)
        .setApiVersion(VK_API_VERSION_1_0);

    // vk::InstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    vk::InstanceCreateInfo instInfo = vk::InstanceCreateInfo()
        .setFlags(vk::InstanceCreateFlags())
        .setPApplicationInfo(&appInfo)
        .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
        .setPpEnabledExtensionNames(extensions.data())
        .setEnabledLayerCount(static_cast<uint32_t>(layers.size()))
        .setPpEnabledLayerNames(layers.data())
        .setPNext(validationFeatures);

    // Create the Vulkan instance.
    instance = vk::createInstance(instInfo);

    VkSurfaceKHR cSurface;
    // Create a Vulkan surface for rendering
    if (!SDL_Vulkan_CreateSurface(window, instance, &cSurface))
        throw std::runtime_error(SDL_GetError());
    surface = cSurface;
}

void Engine::Run()
{
    SDL_Event event;
    bool quit = false;
    while (!quit)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                quit = true;
        }

        std::this_thread::sleep_for(100ms);
    }
}

Engine::~Engine()
{
    instance.destroySurfaceKHR(surface);
    instance.destroy();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
