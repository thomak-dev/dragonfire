#pragma once

#include "Camera.h"
#include "Engine.h"

class Game : public Engine
{
public:
    Game(std::string_view title);

    virtual bool Update(double dt) override;

private:
    Camera camera{};
};
