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
}

EngineBase::~EngineBase()
{
    SDL_DestroyWindow(window);
    SDL_Quit();
    instance = nullptr;
}
