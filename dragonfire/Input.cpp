#include "pch.h"

#include "Input.h"

Input::Input() noexcept
{
    int numKeys{};
    std::ignore = SDL_GetKeyboardState(&numKeys);
    keyStateBefore.resize(numKeys);
}

void Input::Update()
{
    std::memcpy(keyStateBefore.data(), SDL_GetKeyboardState(nullptr), keyStateBefore.size());
}
