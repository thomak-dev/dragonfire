#include "pch.h"

#include "Input.h"

Input::Input()
{
    int numKeys{};
    std::ignore = SDL_GetKeyboardState(&numKeys);
    keyStateBefore.resize(numKeys);
}

void Input::Update() noexcept
{
    std::memcpy(keyStateBefore.data(), SDL_GetKeyboardState(nullptr), keyStateBefore.size());
}
