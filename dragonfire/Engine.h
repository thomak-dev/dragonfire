#pragma once
#include <memory>
#include <string_view>

#include <SDL2/SDL.h>

#include "Graphics.h"

class Engine
{
public:
    Engine(std::string_view title);
    ~Engine();
    void Run();

private:
    SDL_Window* window{};
    std::unique_ptr<Graphics> graphics;
};
