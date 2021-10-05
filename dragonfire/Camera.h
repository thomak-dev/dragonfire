#pragma once
#include <glm/glm.hpp>

struct Camera
{
    glm::vec3 position;
    glm::quat rotation = glm::quat_identity<float, glm::defaultp>();
};
