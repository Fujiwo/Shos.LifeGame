#include "../Shos.LifeGame/ShosLifeGame.h"
#include "../Shos.LifeGame/ShosStopwatch.h"
#include <thread>
#include <vector>
using namespace std;

// Result:
//       (23.363s.)
// FAST: ( 9.109s.)
// MT  : ( 3.270s.)

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

            Shos::stopwatch_viewer stopwatch_viewer;
            for (auto count = 0; count < times; count++)
                game.Next();
        }
    };
}

int main()
{ Shos::LifeGame::Test::Program().Run(); }
