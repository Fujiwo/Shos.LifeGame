// Benchmark of Game::Next for Shos.LifeGame.
//
// The optimization options (USEBITS / FAST / MT / AREA) are chosen from outside by defining
// SHOS_LIFEGAME_CUSTOM_CONFIG, as in the regression tests. See run_benchmark.sh.
//
// Usage: Shos.LifeGame.Benchmark [CellData folder] [scale]
//   scale  Multiplies the number of the generations of each case (default: 1.0).

#include "../Shos.LifeGame/ShosLifeGame.h"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace Shos::LifeGame::Benchmark {

using namespace Shos::LifeGame;

const Size boardSize(1000, 1000); // The same as the application (Shos.LifeGame.cpp)

std::filesystem::path cellDataFolder;
double                scale = 1.0;

std::string GetConfiguration()
{
    std::string configuration;
#if defined(USEBITS)
    configuration += "USEBITS ";
#endif // USEBITS
#if defined(FAST)
    configuration += "FAST ";
#endif // FAST
#if defined(MT)
    configuration += "MT ";
#endif // MT
#if defined(AREA)
    configuration += "AREA ";
#endif // AREA
    return configuration.empty() ? "(none)" : configuration;
}

/// Random cells with a fixed seed, so that every run computes the same generations.
Pattern MakeRandomPattern(const Size& size, unsigned int seed)
{
    std::mt19937 engine(seed);
    std::string  text(size_t(size.cx) * size_t(size.cy), '.');
    for (auto& character : text)
        character = engine() % 2 == 0 ? '*' : '.';
    return Pattern(_T("random"), text, UnsignedInteger(size.cx));
}

/// A glider at the center: AREA computes only a few cells.
Pattern MakeGliderPattern()
{ return Pattern(_T("glider"), ".*...****", 3U); }

std::optional<Pattern> ReadPattern(const std::string& name)
{
    PatternSet patternSet(cellDataFolder.string());
    for (size_t index = 0; index < patternSet.GetSize(); index++) {
        if (patternSet[index].GetName() == name)
            return patternSet[index];
    }
    return std::nullopt;
}

/// Runs the generations from the pattern, and returns the seconds.
std::optional<double> Measure(const Pattern& pattern, int generationNumber)
{
    auto game = std::make_unique<Game>(boardSize);
    if (!game->GetBoard().Set(pattern))
        return std::nullopt;

    const auto start = std::chrono::steady_clock::now();
    for (auto generation = 0; generation < generationNumber; generation++)
        game->Next();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

/// Runs the generations several times, and prints the best time and the generations per second.
/// The best one is used because the time is easily disturbed by other processes (e.g. on WSL).
void Run(const std::string& name, const std::optional<Pattern>& pattern, int generationNumber)
{
    constexpr auto trialNumber = 3;

    generationNumber = std::max(1, int(generationNumber * scale));

    if (!pattern) {
        std::cout << "  " << std::left << std::setw(16) << name << "  (pattern not found)\n";
        return;
    }

    std::optional<double> seconds;
    for (auto trial = 0; trial < trialNumber; trial++) {
        const auto trialSeconds = Measure(*pattern, generationNumber);
        if (!trialSeconds) {
            std::cout << "  " << std::left << std::setw(16) << name << "  (pattern too large)\n";
            return;
        }
        seconds = seconds ? std::min(*seconds, *trialSeconds) : *trialSeconds;
    }

    std::cout << "  " << std::left << std::setw(16) << name
              << std::right << std::setw(7) << generationNumber << " gen  "
              << std::fixed << std::setprecision(3) << std::setw(9) << *seconds * 1000.0 / generationNumber << " ms/gen  "
              << std::setprecision(1) << std::setw(10) << generationNumber / *seconds << " gen/s\n";
}

int Run()
{
    std::cout << "Configuration: " << GetConfiguration() << "\n";
    Run("Random"     , MakeRandomPattern(boardSize, 1U), 200 );
    Run("Glider"     , MakeGliderPattern()             , 2000);
    Run("Gun_Gunstar", ReadPattern("Gun_Gunstar")      , 500 );
    Run("Max"        , ReadPattern("Max")              , 500 );
    return 0;
}

} // namespace Shos::LifeGame::Benchmark

int main(int argc, char* argv[])
{
    using namespace Shos::LifeGame::Benchmark;
    cellDataFolder = argc > 1 ? std::filesystem::path(argv[1])
                              : std::filesystem::path(__FILE__).parent_path().parent_path() / "Shos.LifeGame" / "CellData";
    if (argc > 2)
        scale = std::stod(argv[2]);
    return Run();
}
