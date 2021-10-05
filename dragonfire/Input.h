#pragma once
class Input
{
public:
    Input() noexcept;
    Input(Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&) = delete;
    Input& operator=(Input&&) = delete;

    void Update();

    bool IsKeyDown(SDL_Scancode scancode) const noexcept { return SDL_GetKeyboardState(nullptr)[scancode]; }
    bool KeyWentDown(SDL_Scancode scancode) const noexcept { return SDL_GetKeyboardState(nullptr)[scancode] && !keyStateBefore[scancode]; }
    bool KeyWentUp(SDL_Scancode scancode) const noexcept { return !SDL_GetKeyboardState(nullptr)[scancode] && keyStateBefore[scancode]; }

private:
    std::vector<uint8_t> keyStateBefore;
};
