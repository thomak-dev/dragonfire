#include "pch.h"

#include "Input.h"

Input::Input() noexcept
{
    int numKeys{};
    std::ignore = SDL_GetKeyboardState(&numKeys);
#pragma warning(suppress : 26447) // resize() only throws when this small startup allocation fails
    keyStateBefore.resize(numKeys);
}

void Input::Update() noexcept
{
    std::memcpy(keyStateBefore.data(), SDL_GetKeyboardState(nullptr), keyStateBefore.size());
}
