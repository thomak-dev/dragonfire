#pragma once

#include "EngineBase.h"
#include "Graphics.h"
#include "Input.h"
#include "ResourceManager.h"

class Engine : public EngineBase
{
public:
    friend gfx::Graphics& Graphics() noexcept;
    friend ResourceManager& Resources() noexcept;
    explicit Engine(std::string_view title);
    Engine(Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&) = delete;
    Engine& operator=(Engine&&) = delete;
    ~Engine();

    static Engine& Instance() noexcept { return *dynamic_cast<Engine*>(instance); }

    void Run();
    virtual bool Update(double dt) = 0;
    void RequestWait() const
    {
#ifndef NDEBUG
        std::cout << "Wait requested." << std::endl;
#endif
        graphics.Device().waitIdle();
    };

protected:
    ResourceManager resources;
    ::Input input;
    gfx::Graphics graphics;
};

inline gfx::Graphics& Graphics() noexcept
{
    return Engine::Instance().graphics;
}

inline ResourceManager& Resources() noexcept
{
    return Engine::Instance().resources;
}