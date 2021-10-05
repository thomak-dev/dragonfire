#pragma once

class EngineBase
{
public:
    EngineBase(std::string_view title);
    EngineBase(EngineBase&) = delete;
    EngineBase(EngineBase&&) = delete;
    EngineBase& operator=(EngineBase&) = delete;
    EngineBase& operator=(EngineBase&&) = delete;
    virtual ~EngineBase();

protected:
    SDL_Window* window{};
    static EngineBase* instance;
};
