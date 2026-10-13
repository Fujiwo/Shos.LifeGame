#include <tchar.h>
#include <sstream>
#include <iomanip>

#include "ShosLifeGameBoardPainter.h"
#include "ShosWin32.h"
#include "ShosStopwatch.h"

using namespace std;

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
    static const TCHAR  title[];
    static const TCHAR  windowClassName[];

    Game      game;
    POINT     paintPosition;
    RECT      renderingArea;
#if defined(TIMER)
    Timer*    timer;
#endif // TIMER
    stopwatch watch;

public:
    MainWindow() : game({ 1000, 1000 }), paintPosition({ 0, 0 }), renderingArea({ 0, 0, 0, 0 })
#if defined(TIMER)
        , timer(nullptr)
#endif // TIMER
    {}

#if defined(TIMER)
    ~MainWindow()
    { delete timer; }
#endif // TIMER

    bool Create(HINSTANCE instanceHandle, int showCommand)
    { return Window::Create(instanceHandle, windowClassName, title, showCommand); }

#if defined(IDLE)
    virtual void OnIdle() override
    { Next(); }
#endif // IDLE

protected:
#if defined(TIMER)
    virtual void OnCreate() override
    { timer = new Timer(*this, timerId, timerTimeoutMilliseconds); }
#endif // TIMER

    virtual void OnSize(SIZE size) override
    {
        UNREFERENCED_PARAMETER(size);

        paintPosition = PaintPosition();
        renderingArea = RenderingArea();
    }

    virtual void OnPaint(HDC deviceContextHandle) override
    { BoardPainter::Paint(deviceContextHandle, paintPosition, game.GetBoard()); }

#if defined(TIMER)
    virtual void OnTimer(int timerId) override
    {
        UNREFERENCED_PARAMETER(timerId);

        Next();
    }
#endif // TIMER

    virtual void OnChar(TCHAR character) override
    { Reset(!game.SetPattern(KeyToIndex(character))); }

    virtual void OnRightButtonUp(const POINT& point) override
    {
        UNREFERENCED_PARAMETER(point);

        Reset(true);
    }

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
        double             elapsed = 0.0;

        double GetFps() { return elapsed > 0.0 ? generation / elapsed : 0.0; }
    };

    static Lap lap;
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

const TCHAR MainWindow::title[]           = _T("Shos.LifeGame"         );
const TCHAR MainWindow::windowClassName[] = _T("ShosLifeGameMainWindow");

#if defined(LAP_TIMES)
MainWindow::Lap MainWindow::lap;
#endif //LAP_TIMES

class Program : public Shos::Win32::Program
{
    MainWindow mainWindow;

protected:
    virtual Window* CreateMainWindow(HINSTANCE instanceHandle, int showCommand) override
    { return mainWindow.Create(instanceHandle, showCommand) ? &mainWindow : nullptr; }
};
} // namespace Shos::LifeGame::Application

int APIENTRY _tWinMain(_In_     HINSTANCE instanceHandle        ,
                       _In_opt_ HINSTANCE previousInstanceHandle,
                       _In_     LPTSTR    commandLine           ,
                       _In_     int       showCommand           )
{
    UNREFERENCED_PARAMETER(previousInstanceHandle);
    UNREFERENCED_PARAMETER(commandLine           );

    // Patterns are read from ".\CellData". Make it relative to the executable, not to where it was started from.
    TCHAR modulePath[MAX_PATH];
    const auto length = ::GetModuleFileName(nullptr, modulePath, MAX_PATH);
    if (0 < length && length < MAX_PATH) {
        const auto separator = _tcsrchr(modulePath, _T('\\'));
        if (separator != nullptr) {
            *separator = _T('\0');
            ::SetCurrentDirectory(modulePath);
        }
    }

    Shos::LifeGame::Application::Program().Run(instanceHandle, showCommand);
    return 0;
}
