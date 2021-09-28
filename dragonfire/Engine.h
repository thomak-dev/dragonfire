#pragma once
#include <string_view>
#include <SDL2/SDL.h>
#include "common.h"
class Engine
{
public:
    Engine(std::string_view title);
    ~Engine();
    void Run();

private:
    SDL_Window* window;
    vk::Instance instance;
    vk::SurfaceKHR surface;
};
