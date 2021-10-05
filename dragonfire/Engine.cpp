#include "pch.h"

#include "Engine.h"

#include "dftime.h"

Engine::Engine(std::string_view title) : EngineBase(title), resources(std::bind(&Engine::RequestWait, this)), graphics(window, title)
{
    SDL_version version;
    SDL_VERSION(&version);
    std::cout << "Compiled against SDL version: " << static_cast<int>(version.major) << '.' << static_cast<int>(version.minor) << '.'
              << static_cast<int>(version.patch) << '\n';
    SDL_GetVersion(&version);
    std::cout << "Actual SDL version: " << static_cast<int>(version.major) << '.' << static_cast<int>(version.minor) << '.' << static_cast<int>(version.patch)
              << '\n';

    int w, h;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);
    std::cout << "SDL_Vulkan_GetDrawableSize: " << w << 'x' << h << std::endl;
}

void Engine::Run()
{
    SDL_Event event{};
    bool quit = false;
    Clock::time_point timeBefore;
    while (!quit)
    {
        const auto now = Clock::now();
        const auto dt = now - timeBefore;
        timeBefore = now;
        const auto dtSeconds = std::chrono::duration_cast<std::chrono::microseconds>(dt).count() / 1000'000.0;
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
                    graphics.OnWindowSizeChanged();
                }
        }
        if (Update(dtSeconds))
            quit = true;
        graphics.Render();
        resources.Update();
        input.Update();
    }
    graphics.Device().waitIdle();
}

Engine::~Engine()
{
    resources.Clear();
}
