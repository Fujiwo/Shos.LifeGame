#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
using namespace std;

#include "ShosLifeGameBoardPainter.h"
#include "ShosWin32.h"

namespace Shos::LifeGame::Application {

using namespace Shos::LifeGame;
using namespace Shos::LifeGame::Win32;
using namespace Shos::Win32;

class MainWindow : public Window
{
    static constexpr int timerTimeoutMilliseconds = 200;

    static const TCHAR  title[];
    static const TCHAR  windowClassName[];

    Game  game;
    POINT paintPosition;

    //static const UINT WM_GAME_NEXT = WM_USER + 101;

public:
    MainWindow() : game({ 640, 640 }), paintPosition()
    {}

    bool Create(HINSTANCE instanceHandle, int showCommand)
    {
        return Window::Create(instanceHandle, windowClassName, title, showCommand);
    }

protected:
    virtual void OnCreate() override
    {
        SetTimer(timerTimeoutMilliseconds);
        //RegisterUserMessage(WM_GAME_NEXT);
        //PostMessage(WM_GAME_NEXT);
    }

    virtual void OnSize(SIZE size) override
    {
        UNREFERENCED_PARAMETER(size);

        paintPosition = PaintPosition();
    }

    virtual void OnPaint(HDC deviceContextHandle) override
    {
        //game.Paint(deviceContextHandle, paintPosition);
        BoardPainter::Paint(deviceContextHandle, paintPosition, game.GetBoard());
    }

    virtual void OnTimer(int timerId) override
    {
        UNREFERENCED_PARAMETER(timerId);

        Next();
    }

    //virtual void OnUserMessage(UINT message, WPARAM wParam, LPARAM lParam) override
    //{
    //    UNREFERENCED_PARAMETER(wParam);
    //    UNREFERENCED_PARAMETER(lParam);
    //                  
    //    switch (message) {
    //        case WM_GAME_NEXT:
    //            Next();
    //            break;
    //    }
    //}

private:
    POINT PaintPosition() const
    {
        const auto clientCenter = Center(GetClientRect());
        const auto boardSize = game.GetBoard().GetSize();
        return { clientCenter.x - boardSize.cx / 2, clientCenter.y - boardSize.cy / 2 };
    }

    static POINT Center(const RECT& rect)
    {
        return { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
    }

    void Next()
    {
        thread thread([&]() {
            game.Next();
        });
        thread.join();
        //Redraw();
        Invalidate(DrawRect());
        //PostMessage(WM_GAME_NEXT);
    }

    //void Redraw() const
    //{
    //    Window::Redraw(DrawRect());
    //}

    RECT DrawRect() const
    {
        return RECT{ paintPosition.x, paintPosition.y, paintPosition.x + game.GetBoard().GetSize().cx, paintPosition.y + game.GetBoard().GetSize().cy };
    }
};

const TCHAR MainWindow::title[]           = _T("Shos.LifeGame");
const TCHAR MainWindow::windowClassName[] = _T("ShosLifeGameMainWindow");

class Program
{
    MainWindow mainWindow;

public:
    Program(HINSTANCE instanceHandle, int showCommand)
    {
        if (mainWindow.Create(instanceHandle, showCommand))
            MessageLoop();
    }

private:
    void MessageLoop()
    {
        MSG message;
        while (::GetMessage(&message, nullptr, 0, 0)) {
            ::TranslateMessage(&message);
            ::DispatchMessage (&message);
        }
    }
};

} // namespace Shos::LifeGame::Application

int APIENTRY _tWinMain(_In_     HINSTANCE instanceHandle        ,
                       _In_opt_ HINSTANCE previousInstanceHandle,
                       _In_     LPWSTR    commandLine           ,
                       _In_     int       showCommand           )
{
    UNREFERENCED_PARAMETER(previousInstanceHandle);
    UNREFERENCED_PARAMETER(commandLine           );

    Shos::LifeGame::Application::Program(instanceHandle, showCommand);
    return 0;
}
