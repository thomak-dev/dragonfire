#include "pch.h"

#include "Game.h"

#pragma warning(push)
#pragma warning(disable : 26461)
int main(int argc, char** argv)
#pragma warning(pop)
{
    std::ignore = argc;
    std::ignore = argv;
    Game game{"Dragonfire"};
    game.Run();
    return 0;
}
