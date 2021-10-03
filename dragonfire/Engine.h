#pragma once

#include "Graphics.h"
#include "ResourceManager.h"

class Engine
{
public:
    explicit Engine(std::string_view title);
    Engine(Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&) = delete;
    Engine& operator=(Engine&&) = delete;
    ~Engine();

    static Engine& Instance() noexcept { return *instance; }
    ResourceManager& Resources() noexcept { return resources; }

    void Run();

private:
    static Engine* instance;
    SDL_Window* window{};
    std::unique_ptr<Graphics> graphics;
    ResourceManager resources;
};
