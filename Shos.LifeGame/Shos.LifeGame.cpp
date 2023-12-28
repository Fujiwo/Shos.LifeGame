#include <tchar.h>
#include <sstream>
#include <iomanip>

#include "ShosLifeGameBoardPainter.h"
#include "ShosWin32.h"
#include "ShosStopwatch.h"

using namespace std;

#if defined(UNICODE) || defined(_UNICODE)
#define tstringstream wstringstream
#else // UNICODE
#define tstringstream stringstream
#endif // UNICODE

namespace Shos::LifeGame::Application {

using namespace Shos;
using namespace Shos::LifeGame;
using namespace Shos::LifeGame::Win32;
using namespace Shos::Win32;

class MainWindow final : public Window
{
#if defined(TIMER)
    static constexpr int timerTimeoutMilliseconds = 100;
#endif // TIMER
    static const TCHAR  title[];
    static const TCHAR  windowClassName[];

    Game   game;
    POINT  paintPosition;
    RECT   renderingArea;
#if defined(TIMER)
    Timer* timer;
#endif // TIMER
    stopwatch stopwatch;

public:
    MainWindow() : game({ 800, 800 }), paintPosition({ 0, 0 }), renderingArea({ 0, 0, 0, 0 })
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
    { timer = new Timer(*this, timerTimeoutMilliseconds); }
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
    {
        //if (0 <= index && index < int(Pattern::Type::PatternCount))
            //game.Set(static_cast<Pattern::Type>(KeyToNumber(key)));
        if (!game.SetPattern(KeyToIndex(character)))
            Reset();
    }

    virtual void OnRightButtonUp(const POINT& point) override
    {
        UNREFERENCED_PARAMETER(point);

        Reset();
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
        if (!stopwatch.is_running())
            stopwatch.start();

        game.Next();
        Invalidate(renderingArea);
        SetTitle();
    }

    void Reset()
    {
        game.Reset();
        stopwatch.start();
    }

    void SetTitle() const
    { SetText(GetTitle()); }

    tstring GetTitle() const
    {
        const auto    generation  = game.GetGeneration();
        const auto    patternName = game.GetPatternName();
        const auto    elapsed     = stopwatch.get_elapsed();
        tstringstream stream;
        stream << _T("Sho's Life Game: generation(")
               << generation
               << _T("), time(")
               << std::fixed
               << std::setprecision(1)
               << elapsed
               << _T("), FPS(")
               << std::setprecision(0)
               << generation / elapsed
               << _T("), pattern(")
               << patternName
               << _T(")");
        return stream.str();
    }
};

const TCHAR MainWindow::title[]           = _T("Shos.LifeGame"         );
const TCHAR MainWindow::windowClassName[] = _T("ShosLifeGameMainWindow");

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
                       _In_     LPWSTR    commandLine           ,
                       _In_     int       showCommand           )
{
    UNREFERENCED_PARAMETER(previousInstanceHandle);
    UNREFERENCED_PARAMETER(commandLine           );

    Shos::LifeGame::Application::Program().Run(instanceHandle, showCommand);
    return 0;
}
