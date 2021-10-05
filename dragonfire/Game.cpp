#include "pch.h"

#include "Game.h"

#include "dfmath.h"
#include "Mesh.h"

namespace fs = std::filesystem;

Game::Game(std::string_view title) : Engine(title)
{
    SDL_SetRelativeMouseMode(SDL_TRUE);

    camera.position = glm::vec3(3, 3, 3);
    camera.rotation = LookAt(glm::vec3(1, 1, 1), glm::vec3(0, 1, 0));

    Mesh mesh(fs::path(BasePath()) /= "assets\\meshes\\monke.gltf");
}

bool Game::Update(double dt)
{
    int x = 0, y = 0;
    const auto buttonState = SDL_GetRelativeMouseState(&x, &y);
    const auto relative = SDL_GetRelativeMouseMode();

    if (input.IsKeyDown(SDL_SCANCODE_SPACE))
        camera.position += glm::rotate(glm::inverse(camera.rotation), glm::vec3(0, 1 * dt, 0));
    if (input.IsKeyDown(SDL_SCANCODE_LCTRL))
        camera.position += glm::rotate(glm::inverse(camera.rotation), glm::vec3(0, -1 * dt, 0));
    if (input.IsKeyDown(SDL_SCANCODE_W))
        camera.position += glm::rotate(glm::inverse(camera.rotation), glm::vec3(0, 0, -1 * dt));
    if (input.IsKeyDown(SDL_SCANCODE_S))
        camera.position += glm::rotate(glm::inverse(camera.rotation), glm::vec3(0, 0, 1 * dt));
    if (input.IsKeyDown(SDL_SCANCODE_D))
        camera.position += glm::rotate(glm::inverse(camera.rotation), glm::vec3(1 * dt, 0, 0));
    if (input.IsKeyDown(SDL_SCANCODE_A))
        camera.position += glm::rotate(glm::inverse(camera.rotation), glm::vec3(-1 * dt, 0, 0));

    if (input.KeyWentDown(SDL_SCANCODE_ESCAPE))
    {
        if (relative)
            SDL_SetRelativeMouseMode(SDL_FALSE);
        else
            return true;
    }

    if (!relative && (buttonState & SDL_BUTTON(SDL_BUTTON_LEFT)))
        SDL_SetRelativeMouseMode(SDL_TRUE);

    int roll = 0;
    if (input.IsKeyDown(SDL_SCANCODE_Q))
        roll -= 1;
    if (input.IsKeyDown(SDL_SCANCODE_E))
        roll += 1;

    if (relative)
        camera.rotation =
            glm::toQuat(glm::yawPitchRoll(x * static_cast<float>(dt), y * static_cast<float>(dt), roll * static_cast<float>(dt))) * camera.rotation;

    graphics.ViewMatrix(glm::toMat4(camera.rotation) * glm::translate(glm::mat4(1), -camera.position));

    return false;
}

void Game::OnWindowSizeChanged(uint32_t width, uint32_t height)
{
    if (width > 0 && height > 0)
    {
        auto proj = glm::perspective(glm::radians(camera.fov), width / static_cast<float>(height), camera.nearPlane, camera.farPlane);
        proj[1][1] *= -1;
        graphics.ProjectionMatrix(proj);
    }
}
