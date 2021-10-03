#include "pch.h"

#include "Engine.h"

#pragma warning(push)
#pragma warning(disable : 26461)
int main(int argc, char** argv)
#pragma warning(pop)
{
    (void)argc, (void)argv;
    Engine engine{"Dragonfire"};
    engine.Run();
    return 0;
}
