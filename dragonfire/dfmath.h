#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

inline glm::quat LookAt(const glm::vec3& dir, const glm::vec3& up)
{
    const auto z = glm::normalize(dir);
    const auto x = glm::normalize(glm::cross(up, z));
    const auto y = glm::cross(z, x);
    glm::mat4 la{1};
    la[0] = glm::vec4(x.x, y.x, z.x, 0);
    la[1] = glm::vec4(x.y, y.y, z.y, 0);
    la[2] = glm::vec4(x.z, y.z, z.z, 0);
    return la;
}
