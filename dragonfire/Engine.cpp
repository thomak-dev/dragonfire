#include "Engine.h"

#include <iostream>

#include <SDL2/SDL_vulkan.h>

static const char* EngineName = "Dragonfire Engine";

Engine::Engine(std::string_view title)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_version version;
    SDL_VERSION(&version);
    std::cout << "Compiled against SDL version: " << (int)version.major << '.' << (int)version.minor << '.' << (int)version.patch << '\n';
    SDL_GetVersion(&version);
    std::cout << "Actual SDL version: " << (int)version.major << '.' << (int)version.minor << '.' << (int)version.patch << '\n';

    window = SDL_CreateWindow(title.data(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
        throw std::runtime_error{SDL_GetError()};

    graphics = std::make_unique<Graphics>(window, title, EngineName);
    int w, h;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);
    std::cout << "SDL_Vulkan_GetDrawableSize: " << w << 'x' << h << std::endl;
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

            if (event.type == SDL_WINDOWEVENT)
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    std::cout << "Window size changed: " << event.window.data1 << 'x' << event.window.data2 << '\n';
                    int w, h;
                    SDL_Vulkan_GetDrawableSize(window, &w, &h);
                    std::cout << "SDL_Vulkan_GetDrawableSize: " << w << 'x' << h << std::endl;
                    graphics->OnWindowSizeChanged();
                }
        }

        graphics->Render();
    }
}

Engine::~Engine()
{
    graphics.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
