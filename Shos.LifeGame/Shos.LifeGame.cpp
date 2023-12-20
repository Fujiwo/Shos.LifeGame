#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <functional>
#include <tuple>
#include <cstdlib>
#include <cassert>
using namespace std;

#ifndef _DEBUG
#define _RPTTN(msg, ...)
#else	// ifndef _DEBUG else
#ifdef UNICODE
#define _RPTTN(msg, ...) _RPT_BASE_W(_CRT_WARN, NULL, 0, NULL, msg, __VA_ARGS__)
#else	// ifdef UNICODE else
#define _RPTTN(msg, ...) _RPT_BASE(_CRT_WARN, NULL, 0, NULL, msg, __VA_ARGS__)
#endif	// ifdef UNICODE
#endif	// ifndef _DEBUG

namespace Shos::LifeGame {

using Integer         = int          ;
using UnsignedInteger = unsigned int ;
using Byte            = unsigned char;
using UnitInteger     = Byte         ;

struct Size
{
    Integer cx;
    Integer cy;

    Size() : cx(0), cy(0)
    {}

    Size(Integer cx, Integer cy) : cx(cx), cy(cy)
    {}

    bool operator ==(const Size& size) const
    {
        return cx == size.cx && cy == size.cy;
    }
};

struct Point
{
    Integer x;
    Integer y;

    Point() : x(0), y(0)
    {}

    Point(Integer x, Integer y) : x(x), y(y)
    {}

    bool operator ==(const Point& point) const
    {
        return x == point.x && y == point.y;
    }

    bool operator !=(const Point& point) const
    {
        return !(*this == point);
    }

    Point operator +(const Size& size) const
    {
        return { x + size.cx, y + size.cy };
    }   
};

struct Rect
{
    Point leftTop;
    Size  size;

    Point RightBottom() const
    {
        return leftTop + size;
    }

    bool IsIn(const Point& point) const
    {
        return IsIn(point.x, leftTop.x, size.cx) &&
               IsIn(point.y, leftTop.y, size.cy);
    }

private:
    template <typename T>
    bool IsIn(T value, T minimum, T size) const
    {
        return minimum <= value && value < minimum + size;
    }
};

class Utility
{
public:
    static UnsignedInteger Count(const Rect& rect, function<bool(const Point&)> isMatch)
    {
        UnsignedInteger count = 0;
        ForEach(rect, [&](const Point& point) {
            if (isMatch(point))
                count++;
        });
        return count;
    }

    static void ForEach(const Rect& rect, function<void(const Point&)> action)
    {
        const Point rightBottom = rect.RightBottom();

        for (auto index = rect.leftTop; index.y < rightBottom.y; index.y++) {
            for (index.x = rect.leftTop.x; index.x < rightBottom.x; index.x++)
                action(index);
        }
    }
};

class Board
{
    const Size      size;
    UnsignedInteger unitNumberX;
    UnitInteger*    cells;

public:
    Size GetSize() const
    {
        return size;
    }

    UnitInteger* GetCells() const
    {
        return cells;
    }

    Board(const Size& size) : size(size)
    {
        Initialize();
    }

    virtual ~Board()
    {
        delete[] cells;
    }

    void CopyTo(Board& board) const
    {
        assert(size == board.size);
        ::CopyMemory(board.cells, cells, GetUnitNumber() * (sizeof(UnitInteger) / sizeof(Byte)));
    }

    void ForEach(function<void(const Point&)> action)
    {
        Utility::ForEach(GetRect(), action);
    }

    UnsignedInteger GetAliveNeighborCount(const Point& point) const
    {
        return Utility::Count(
            Rect(point + Size(-1, -1), Size(3, 3)),
            [&](const Point& neighborPoint) {
                return neighborPoint != point && Get(neighborPoint);
            }
        );
    }

    bool Get(const Point& point) const
    {
        tuple<UnsignedInteger, Byte> bitIndex;
        if (!ToIndex(point, bitIndex))
            return false;

        const auto [index, bit] = bitIndex;
        return (cells[index] & (1 << bit)) != 0;
    }

    void Set(const Point& point, bool value)
    {
        tuple<UnsignedInteger, Byte> bitIndex;
        if (!ToIndex(point, bitIndex))
            return;

        const auto [index, bit] = bitIndex;
        value ? (cells[index] |=   1 << bit )
              : (cells[index] &= ~(1 << bit));
    }

private:
    void Initialize()
    {
        InitializeUnitNumberX();
        const auto unitNumber = GetUnitNumber();
        cells                 = new UnitInteger[unitNumber];
        ::ZeroMemory(cells, unitNumber * (sizeof(UnitInteger) / sizeof(Byte)));
    }

    void InitializeUnitNumberX()
    {
        unitNumberX = size.cx / sizeof(UnitInteger);
        if (size.cx % sizeof(UnitInteger) != 0)
            unitNumberX++;
    }

    UnsignedInteger GetUnitNumber() const
    {
        return unitNumberX * size.cy;
    }

    bool ToIndex(const Point& point, tuple<UnsignedInteger, Byte>& bitIndex) const
    {
        if (!GetRect().IsIn(point))
            return false;

        constexpr auto bitNumber = sizeof(UnitInteger) * 8;
        const auto index         = UnsignedInteger(unitNumberX * point.y + point.x / bitNumber);
        const auto bit           = Byte(point.x % bitNumber);
        bitIndex                 = { index, bit };
        return true;
    }

    Rect GetRect() const
    {
        return Rect(Point(), size);
    }
};

class BoardPainter
{
public:
    static void Paint(HDC deviceContextHandle, const POINT& position, const Board& board)
    {
        const auto bitmapHandle = CreateBitmap(board);
        Paint(deviceContextHandle, position, { board.GetSize().cx, board.GetSize().cy }, bitmapHandle);
        ::DeleteObject(bitmapHandle);
    }

private:
    static HBITMAP CreateBitmap(const Board& board)
    {
        BITMAP bitmap;
        ::ZeroMemory(&bitmap, sizeof(BITMAP));
        bitmap.bmWidth      = board.GetSize().cx;
        bitmap.bmHeight     = board.GetSize().cy;
        bitmap.bmPlanes     = 1;
        bitmap.bmWidthBytes = board.GetSize().cx / sizeof(Byte);
        bitmap.bmBitsPixel  = 1;
        bitmap.bmBits       = board.GetCells();

        return ::CreateBitmapIndirect(&bitmap);
    }

    static void Paint(HDC deviceContextHandle, const POINT& position, const SIZE& size, HBITMAP bitmapHandle)
    {
        const auto memoryDeviceContextHandle = ::CreateCompatibleDC(deviceContextHandle);
        ::SelectObject(memoryDeviceContextHandle, bitmapHandle);
        ::BitBlt(deviceContextHandle, position.x, position.y, size.cx, size.cy, memoryDeviceContextHandle, 0, 0, SRCCOPY);
        ::DeleteDC(memoryDeviceContextHandle);
    }
};

class Game
{
    Board* mainBoard;
    Board* subBoard ;

public:
    Board& GetBoard()
    {
        return *mainBoard;
    }

    Game(const Size& size) : mainBoard(new Board(size)), subBoard(new Board(size))
    {
        Initialize();
    }

    virtual ~Game()
    {
        delete subBoard ;
        delete mainBoard;
    }

    void Paint(HDC deviceContextHandle, const POINT& position)
    {
        BoardPainter::Paint(deviceContextHandle, position, *mainBoard);
    }

    void Next()
    {
        mainBoard->CopyTo(*subBoard);
        mainBoard->ForEach([&](const Point& point) {
            const auto aliveNeighborCount = mainBoard->GetAliveNeighborCount(point);
            const auto alive              = mainBoard->Get(point);
            subBoard->Set(point, aliveNeighborCount == 3 || (aliveNeighborCount == 2 && alive));
        });
        swap(mainBoard, subBoard);
    }

private:
    void Initialize()
    {
        mainBoard->ForEach([&](const Point& point) {
            mainBoard->Set(point, rand() % 2 == 0);
        });
    }
};

class Window
{
    HWND handle;

public:
    Window() : handle(nullptr)
    {}

    bool Create(HINSTANCE instanceHandle, LPCTSTR windowClassName, LPCTSTR title, int showCommand)
    {
        RegisterWindowClass(instanceHandle, windowClassName);

        handle = ::CreateWindow(windowClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, instanceHandle, this);

        if (handle == nullptr)
            return false;

        ::ShowWindow(handle, showCommand);
        ::UpdateWindow(handle);
        return true;
    }

    void SetTimer(int timerId = 1, UINT timeoutMilliseconds = 1000)
    {
        ::SetTimer(handle, timerId, timeoutMilliseconds, nullptr);
    }

    void Invalidate() const
    {
        ::InvalidateRect(handle, nullptr, FALSE);
    }
    
    RECT GetClientRect() const
    {
        RECT clientRect;
        ::GetClientRect(handle, &clientRect);
        return clientRect;
    }

protected:
    virtual void OnCreate()
    {}

    virtual void OnSize(SIZE size)
    {
        UNREFERENCED_PARAMETER(size);
    }

    virtual void OnPaint(HDC deviceContextHandle)
    {
        UNREFERENCED_PARAMETER(deviceContextHandle);
    }

    virtual void OnTimer(int timerId)
    {
        UNREFERENCED_PARAMETER(timerId);
    }

private:
    static void SetSelf(HWND windowHandle, Window* self)
    {
        ::SetWindowLongPtr(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    static Window* GetSelf(HWND windowHandle)
    {
        return reinterpret_cast<Window*>(::GetWindowLongPtr(windowHandle, GWLP_USERDATA));
    }

    static LRESULT CALLBACK windowProcedure(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message) {
            case WM_CREATE: {
                auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
                auto self = reinterpret_cast<Window*>(createStruct->lpCreateParams);
                self->handle = windowHandle;
                SetSelf(windowHandle, self);
                self->OnCreate();
            }
            break;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                auto deviceContextHandle = ::BeginPaint(windowHandle, &ps);
                GetSelf(windowHandle)->OnPaint(deviceContextHandle);
                ::EndPaint(windowHandle, &ps);
            }
            break;
            case WM_SIZE: {
                const auto size = SIZE { LOWORD(lParam), HIWORD(lParam) };
                GetSelf(windowHandle)->OnSize(size);
            }
            break;
            case WM_TIMER: {
                GetSelf(windowHandle)->OnTimer(int(wParam));
            }
            break;
            case WM_DESTROY:
                ::PostQuitMessage(0);
                break;
            default:
                return ::DefWindowProc(windowHandle, message, wParam, lParam);
        }
        return 0;
    }

    static bool RegisterWindowClass(HINSTANCE instanceHandle, LPCTSTR windowClassName)
    {
        WNDCLASSEXW wcex;
        if (::GetClassInfoEx(instanceHandle, windowClassName, &wcex))
            return true;

        ::ZeroMemory(&wcex, sizeof(WNDCLASSEXW));
        wcex.cbSize        = sizeof(WNDCLASSEX);
        wcex.style         = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc   = windowProcedure;
        wcex.hInstance     = instanceHandle;
        wcex.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszClassName = windowClassName;
        return ::RegisterClassEx(&wcex) != 0;
    }
};

class MainWindow : public Window
{
    static constexpr int timerTimeoutMilliseconds = 100;

    static const TCHAR  title          [];
    static const TCHAR  windowClassName[];

    Game game;
    POINT paintPosition;

public:
    MainWindow() : game({ 400, 400 }), paintPosition()
    {}

    bool Create(HINSTANCE instanceHandle, int showCommand)
    {
        return Window::Create(instanceHandle, windowClassName, title, showCommand);
    }

protected:
    virtual void OnCreate()
    {
        SetTimer(timerTimeoutMilliseconds);
    }

    virtual void OnSize(SIZE size)
    {
        UNREFERENCED_PARAMETER(size);

        paintPosition = PaintPosition();
    }

    virtual void OnPaint(HDC deviceContextHandle) override
    {
        game.Paint(deviceContextHandle, paintPosition);
    }

    virtual void OnTimer(int timerId)
    {
        UNREFERENCED_PARAMETER(timerId);

        game.Next();
        Invalidate();
    }

private:
    POINT PaintPosition()
    {
        const auto clientCenter = Center(GetClientRect());
        const auto boardSize    = game.GetBoard().GetSize();
        return { clientCenter.x - boardSize.cx / 2, clientCenter.y - boardSize.cy / 2 };
    }

    static POINT Center(const RECT& rect)
    {
        return { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
    }
};

const TCHAR MainWindow::title          [] = _T("Shos.LifeGame"         );
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

} // Shos::LifeGame

int APIENTRY wWinMain(_In_     HINSTANCE instanceHandle        ,
                      _In_opt_ HINSTANCE previousInstanceHandle,
                      _In_     LPWSTR    commandLine           ,
                      _In_     int       showCommand           )
{
    UNREFERENCED_PARAMETER(previousInstanceHandle);
    UNREFERENCED_PARAMETER(commandLine           );

    Shos::LifeGame::Program(instanceHandle, showCommand);
    return 0;
}
