#include <tchar.h>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>

#include "ShosLifeGameBoardPainter.h"
#include "ShosWin32.h"
#include "ShosStopwatch.h"

//#define LAP_TIMES   1000

namespace Shos::LifeGame::Application {

using tstringstream = std::basic_stringstream<TCHAR>;

using namespace Shos;
using namespace Shos::LifeGame;
using namespace Shos::LifeGame::Win32;
using namespace Shos::Win32;

class MainWindow final : public Window
{
#if defined(TIMER)
    static constexpr UINT_PTR timerId                  = 1U;
    static constexpr UINT     timerTimeoutMilliseconds = 100U;
#endif // TIMER
    static constexpr TCHAR title[]           = _T("Shos.LifeGame"         );
    static constexpr TCHAR windowClassName[] = _T("ShosLifeGameMainWindow");

    Game                   game{ Size(1000, 1000) };
    POINT                  paintPosition{};
    RECT                   renderingArea{};
#if defined(TIMER)
    std::unique_ptr<Timer> timer;
#endif // TIMER
    stopwatch              watch;

public:
    bool Create(HINSTANCE instanceHandle, int showCommand)
    { return Window::Create(instanceHandle, windowClassName, title, showCommand); }

#if defined(IDLE)
    void OnIdle() override
    { Next(); }
#endif // IDLE

protected:
#if defined(TIMER)
    void OnCreate() override
    { timer = std::make_unique<Timer>(*this, timerId, timerTimeoutMilliseconds); }
#endif // TIMER

    void OnSize([[maybe_unused]] SIZE size) override
    {
        paintPosition = PaintPosition();
        renderingArea = RenderingArea();
    }

    void OnPaint(HDC deviceContextHandle) override
    { BoardPainter::Paint(deviceContextHandle, paintPosition, game.GetBoard()); }

#if defined(TIMER)
    void OnTimer([[maybe_unused]] int timerId) override
    { Next(); }
#endif // TIMER

    void OnChar(TCHAR character) override
    { Reset(!game.SetPattern(KeyToIndex(character))); }

    void OnRightButtonUp([[maybe_unused]] const POINT& point) override
    { Reset(true); }

private:
    static int KeyToIndex(TCHAR character)
    { return _istalpha(character) ? AlphaToIndex(character) : (_istdigit(character) ? character - '0' : -1); }

    static int AlphaToIndex(TCHAR character)
    { return _istlower(character) ? character - 'a' + 10 : character - 'A' + 10 + 26; }

    POINT PaintPosition() const
    {
        const auto clientCenter = Center(GetClientRect());
        const auto boardSize    = game.GetBoard().GetSize();
        return { clientCenter.x - boardSize.cx / 2, clientCenter.y - boardSize.cy / 2 };
    }

    RECT RenderingArea() const
    { return RECT{ paintPosition.x, paintPosition.y, paintPosition.x + game.GetBoard().GetSize().cx, paintPosition.y + game.GetBoard().GetSize().cy }; }

    static POINT Center(const RECT& rect)
    { return { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 }; }

    void Next()
    {
        if (!watch.is_running())
            watch.start();

        game.Next();
        Invalidate(renderingArea);
        SetTitle();
    }

    void Reset(bool randomize)
    {
        game.Reset(randomize);
        watch.start();
    }

    void SetTitle() const
    { SetText(GetTitle()); }

#if defined(LAP_TIMES)
    struct Lap
    {
        unsigned long long generation = 0;
        double             elapsed    = 0.0;

        double GetFps() const { return elapsed > 0.0 ? generation / elapsed : 0.0; }
    };

    static inline Lap lap;
#endif //LAP_TIMES

    tstring GetTitle() const
    {
        auto    generation  = game.GetGeneration();
        auto    patternName = game.GetPatternName();
        auto    elapsed     = watch.get_elapsed();
#if defined(LAP_TIMES)
        if (generation == LAP_TIMES) {
            lap.generation = generation;
            lap.elapsed    = elapsed   ;
        } else if (generation > LAP_TIMES) {
            generation     = lap.generation;
            elapsed        = lap.elapsed   ;
        }
#endif //LAP_TIMES
        tstringstream stream;
        stream << _T("Sho's Life Game: generation(")
               << generation
               << _T("), time(")
               << std::fixed
               << std::setprecision(1)
               << elapsed
               << _T("), FPS(")
               << std::setprecision(0)
               << (elapsed > 0.0 ? generation / elapsed : 0.0)
               << _T("), pattern(")
               << patternName
               << _T(")");
        return stream.str();
    }
};

class Program : public Shos::Win32::Program
{
    MainWindow mainWindow;

protected:
    Window* CreateMainWindow(HINSTANCE instanceHandle, int showCommand) override
    { return mainWindow.Create(instanceHandle, showCommand) ? &mainWindow : nullptr; }
};
} // namespace Shos::LifeGame::Application

int APIENTRY _tWinMain(                  _In_     HINSTANCE instanceHandle        ,
                       [[maybe_unused]] _In_opt_ HINSTANCE previousInstanceHandle,
                       [[maybe_unused]] _In_     LPTSTR    commandLine           ,
                                        _In_     int       showCommand           )
{
    // Patterns are read from ".\CellData". Make it relative to the executable, not to where it was started from.
    TCHAR modulePath[MAX_PATH]{};
    const auto length = ::GetModuleFileName(nullptr, modulePath, MAX_PATH);
    if (0 < length && length < MAX_PATH) {
        std::error_code error;
        std::filesystem::current_path(std::filesystem::path(modulePath).parent_path(), error);
    }

    Shos::LifeGame::Application::Program().Run(instanceHandle, showCommand);
    return 0;
}
