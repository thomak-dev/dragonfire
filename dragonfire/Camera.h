#pragma once
#include <glm/glm.hpp>

struct Camera
{
    glm::vec3 position;
    glm::quat rotation = glm::quat_identity<float, glm::defaultp>();
    float fov{60};
    float nearPlane{0.03f};
    float farPlane{1000.f};
};
