#pragma once

#include "Camera.h"
#include "Engine.h"

class Game : public Engine
{
public:
    explicit Game(std::string_view title);

    bool Update(double dt) override;
    void OnWindowSizeChanged(uint32_t width, uint32_t height) noexcept override;

private:
    Camera camera{};
};
