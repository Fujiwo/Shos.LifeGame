#pragma once

//#define USERMESSAGE
//#define TIMER
#define IDLE

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

namespace Shos::Win32 {

class Window
{
    HWND handle;

#if defined(USERMESSAGE)
    vector<UINT> userMessages;
#endif // USERMESSAGE

public:
    HWND GetHandle() const
    { return this == nullptr ? nullptr : handle; }

    Window() : handle(nullptr)
    {}

    virtual ~Window()
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

#if defined(TIMER)
    UINT_PTR SetTimer(UINT_PTR timerId = 1U, UINT timeoutMilliseconds = 1000)
    { return ::SetTimer(handle, timerId, timeoutMilliseconds, nullptr); }

    UINT_PTR KillTimer(UINT_PTR timerId = 1U, UINT timeoutMilliseconds = 1000)
    { return ::SetTimer(handle, timerId, timeoutMilliseconds, nullptr); }
#endif // TIMER

    void Invalidate(const RECT& rect) const
    { ::InvalidateRect(handle, &rect, FALSE); }

    //void Invalidate() const
    //{ ::InvalidateRect(handle, nullptr, FALSE); }
    
    RECT GetClientRect() const
    {
        RECT clientRect;
        ::GetClientRect(handle, &clientRect);
        return clientRect;
    }

    void SetText(LPCTSTR text) const
    { ::SetWindowText(handle, text); }

#if defined(USERMESSAGE)
    void PostMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
    { ::PostMessage(handle, message, wParam, lParam); }

    void RegisterUserMessage(UINT userMessage)
    {
        if (!ExistsUserMessage(userMessage))
            userMessages.push_back(userMessage);
    }
#endif // USERMESSAGE

#if defined(IDLE)
    virtual void OnIdle()
    {}
#endif // IDLE

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

#if defined(TIMER)
    virtual void OnTimer(int timerId)
    {
        UNREFERENCED_PARAMETER(timerId);
    }
#endif // TIMER

    virtual void OnUserMessage(UINT message, WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER(message);
        UNREFERENCED_PARAMETER(wParam );
        UNREFERENCED_PARAMETER(lParam );
    }

private:
    static void SetSelf(HWND windowHandle, Window* self)
    { ::SetWindowLongPtr(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self)); }

    static Window* GetSelf(HWND windowHandle)
    { return reinterpret_cast<Window*>(::GetWindowLongPtr(windowHandle, GWLP_USERDATA)); }

    static LRESULT CALLBACK windowProcedure(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message) {
            case WM_CREATE: {
                auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
                auto self         = reinterpret_cast<Window*>(createStruct->lpCreateParams);
                self->handle      = windowHandle;
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
#if defined(TIMER)
            case WM_TIMER:
                GetSelf(windowHandle)->OnTimer(int(wParam));
                break;
#endif // TIMER
            case WM_DESTROY:
                ::PostQuitMessage(0);
                break;
            default:
#if defined(USERMESSAGE)
                {
                    auto self = GetSelf(windowHandle);
                    if (self == nullptr || !self->OnUserMessage(message))
                        return ::DefWindowProc(windowHandle, message, wParam, lParam);
                }
                break;
#else // USERMESSAGE
                return ::DefWindowProc(windowHandle, message, wParam, lParam);
#endif // USERMESSAGE
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

#if defined(USERMESSAGE)
    bool OnUserMessage(UINT userMessage)
    {
        if (ExistsUserMessage(userMessage)) {
            OnUserMessage(userMessage, 0, 0);
            return true;
        }
        return false;
    }

    bool ExistsUserMessage(UINT userMessage)
    { return find(userMessages.begin(), userMessages.end(), userMessage) != userMessages.end(); }
#endif // USERMESSAGE
};

#if defined(TIMER)
class Timer final
{
    const UINT_PTR handle;

public:
    Timer(const Window& window, UINT_PTR timerId = 1U, UINT timeoutMilliseconds = 1000)
        : handle(::SetTimer(window.GetHandle(), timerId, timeoutMilliseconds, nullptr))
    {}

    ~Timer()
    { ::KillTimer(nullptr, handle); }
};
#endif // TIMER

class Program
{
    Window* mainWindow;

public:
    Program() : mainWindow(nullptr)
    {}

    virtual ~Program()
    {}

    bool Run(HINSTANCE instanceHandle, int showCommand)
    {
        mainWindow = CreateMainWindow(instanceHandle, showCommand);
        if (mainWindow == nullptr)
            return false;
        MessageLoop();
        return true;
    }

protected:
    virtual Window* CreateMainWindow(HINSTANCE instanceHandle, int showCommand) = 0;

private:
    void MessageLoop()
    {
        MSG message;
#if defined(IDLE)
        for (; ;) {
            if (::PeekMessage(&message, NULL, 0U, 0U, PM_REMOVE)) {
                if (message.message == WM_QUIT)
                    break;
                ::TranslateMessage(&message);
                ::DispatchMessage (&message);
            } else {
                if (mainWindow != nullptr)
                    mainWindow->OnIdle();
            }
        }
#else // IDLE
        while (::GetMessage(&message, nullptr, 0, 0)) {
            ::TranslateMessage(&message);
            ::DispatchMessage (&message);
        }
#endif // IDLE
    }
};

} // namespace Shos::Win32
