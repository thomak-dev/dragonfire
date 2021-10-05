#include "pch.h"

#include "Engine.h"

#include "dftime.h"

namespace fs = std::filesystem;

Engine::Engine(std::string_view title) : EngineBase(title), resources(std::bind(&Engine::RequestWait, this)), graphics(window, title)
{
    int w, h;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);
    std::cout << "SDL_Vulkan_GetDrawableSize: " << w << 'x' << h << std::endl;

    SDL_SysWMinfo windowInfo;
    SDL_VERSION(&windowInfo.version);
    SDL_GetWindowWMInfo(window, &windowInfo);
    uint32_t dpi = 96;
#if _WIN32
    dpi = GetDpiForWindow(windowInfo.info.win.window);
#endif
    graphics.OnDpiChanged(dpi);
    std::cout << "DPI: " << dpi << std::endl;
}

void Engine::Run()
{
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    SDL_Event event{};
    bool quit = false;
    Clock::time_point timeBefore;
    OnWindowSizeChanged(graphics.Width(), graphics.Height());

    while (!quit)
    {
        const auto now = Clock::now();
        const auto dt = now - timeBefore;
        timeBefore = now;
        const auto dtSeconds = std::chrono::duration_cast<std::chrono::microseconds>(dt).count() / 1000'000.0;
        uint32_t newDpi = 0;
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
                    OnWindowSizeChanged(w, h);
                }
#if _WIN32
            if (event.type == SDL_SYSWMEVENT)
            {
                if (event.syswm.msg->msg.win.msg == WM_DPICHANGED)
                {
                    newDpi = LOWORD(event.syswm.msg->msg.win.wParam);
                }
            }
#endif
        }
        // process new DPI after we emptied the event queue because it's possible to get multiple WM_DPICHANGED queued up in a single frame
        if (newDpi != 0 && newDpi != graphics.Dpi())
        {
            graphics.OnDpiChanged(newDpi);
            std::cout << "New DPI: " << newDpi << std::endl;
            // imGuiManager.OnDpiChanged(newDpi);
        }
        if (Update(dtSeconds))
            quit = true;
        graphics.Render();
        resources.Update();
        input.Update();
    }
    RequestWait();
}

std::string Engine::LoadBinaryFile(const std::filesystem::path& relativePath)
{
    fs::path fullPath{Engine::Instance().BasePath()};
    fullPath /= relativePath.lexically_normal();
    std::ifstream file{fullPath, std::ios::binary};
    std::stringstream strStream;
    strStream << file.rdbuf();
    return strStream.str();
}

Engine::~Engine()
{
    resources.Clear();
}
