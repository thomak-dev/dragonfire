#pragma once

class EngineBase
{
public:
    explicit EngineBase(std::string_view title);
    EngineBase(EngineBase&) = delete;
    EngineBase(EngineBase&&) = delete;
    EngineBase& operator=(EngineBase&) = delete;
    EngineBase& operator=(EngineBase&&) = delete;
    virtual ~EngineBase();

    std::string_view BasePath() const noexcept { return basePath; }

protected:
    SDL_Window* window{};
    static EngineBase* instance;

private:
    char* basePath{};
};
