#include "pch.h"

#include "EngineBase.h"

EngineBase* EngineBase::instance{};

EngineBase::EngineBase(std::string_view title)
{
    if (instance)
        throw std::runtime_error{"There can be only one Engine instance."};

    instance = this;
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(title.data(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
        throw std::runtime_error{SDL_GetError()};

    SDL_version version;
    SDL_VERSION(&version);
    std::cout << "Compiled against SDL version: " << static_cast<int>(version.major) << '.' << static_cast<int>(version.minor) << '.'
              << static_cast<int>(version.patch) << '\n';
    SDL_GetVersion(&version);
    std::cout << "Actual SDL version: " << static_cast<int>(version.major) << '.' << static_cast<int>(version.minor) << '.' << static_cast<int>(version.patch)
              << '\n';

    basePath = SDL_GetBasePath();
}

EngineBase::~EngineBase()
{
    SDL_free(basePath);
    SDL_DestroyWindow(window);
    SDL_Quit();
    instance = nullptr;
}
