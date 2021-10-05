#pragma once

#include "Camera.h"
#include "Engine.h"

class Game : public Engine
{
public:
    Game(std::string_view title);

    virtual bool Update(double dt) override;
    virtual void OnWindowSizeChanged(uint32_t width, uint32_t height) override;

private:
    Camera camera{};
};
