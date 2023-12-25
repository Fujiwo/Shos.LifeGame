#include "../Shos.LifeGame/ShosLifeGame.h"

#include "ShosStopwatch.h"
#include <thread>
#include <vector>
using namespace std;

// Result:
//       (27.637s.)
// FAST: (11.860s.)
// MT  : ( 5.990s.)

namespace Shos::LifeGame::Test {
    using namespace Shos::LifeGame;

    class Program
    {
    public:
        void Run()
        {
            const Integer size = 2048;
            const size_t times =  100;

            Shos::LifeGame::Game game({ size, size });

            Shos::stopwatch stopwatch;
            for (auto count = 0; count < times; count++)
                game.Next();
        }
    };
}

int main()
{ Shos::LifeGame::Test::Program().Run(); }
