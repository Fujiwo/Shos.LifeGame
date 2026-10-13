#pragma once

//#define USERMESSAGE // OnUserMessage enabled
//#define TIMER       // OnTimer enabled
#define IDLE        // OnIdle enabled

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#if defined(USERMESSAGE)
#include <algorithm>
#include <vector>
#endif // USERMESSAGE
#include "ShosHelper.h"

namespace Shos::Win32 {

class Window
{
    HWND handle = nullptr;

#if defined(USERMESSAGE)
    std::vector<UINT> userMessages;
#endif // USERMESSAGE

public:
    HWND GetHandle() const
    { return handle; }

    Window() = default;

    virtual ~Window() = default;

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

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

    bool KillTimer(UINT_PTR timerId = 1U)
    { return ::KillTimer(handle, timerId) != FALSE; }
#endif // TIMER

    void Invalidate(const RECT& rect) const
    { ::InvalidateRect(handle, &rect, FALSE); }
    
    RECT GetClientRect() const
    {
        RECT clientRect{};
        ::GetClientRect(handle, &clientRect);
        return clientRect;
    }

    void SetText(const tstring& text) const
    { ::SetWindowText(handle, text.c_str()); }

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

    virtual void OnSize([[maybe_unused]] SIZE size)
    {}

    virtual void OnPaint([[maybe_unused]] HDC deviceContextHandle)
    {}

#if defined(TIMER)
    virtual void OnTimer([[maybe_unused]] int timerId)
    {}
#endif // TIMER

    virtual void OnChar([[maybe_unused]] TCHAR character)
    {}

    virtual void OnRightButtonUp([[maybe_unused]] const POINT& point)
    {}

    virtual void OnUserMessage([[maybe_unused]] UINT message, [[maybe_unused]] WPARAM wParam, [[maybe_unused]] LPARAM lParam)
    {}

private:
    static void SetSelf(HWND windowHandle, Window* self)
    { ::SetWindowLongPtr(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self)); }

    static Window* GetSelf(HWND windowHandle)
    { return reinterpret_cast<Window*>(::GetWindowLongPtr(windowHandle, GWLP_USERDATA)); }

    static LRESULT CALLBACK windowProcedure(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message) {
            case WM_CREATE: {
                    const auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
                    const auto self         = static_cast<Window*>(createStruct->lpCreateParams);
                    self->handle            = windowHandle;
                    SetSelf(windowHandle, self);
                    self->OnCreate();
                }
                break;
            case WM_PAINT: {
                    PAINTSTRUCT ps{};
                    const auto deviceContextHandle = ::BeginPaint(windowHandle, &ps);
                    GetSelf(windowHandle)->OnPaint(deviceContextHandle);
                    ::EndPaint(windowHandle, &ps);
                }
                break;
            case WM_SIZE: {
                    const auto size = SIZE{ LOWORD(lParam), HIWORD(lParam) };
                    GetSelf(windowHandle)->OnSize(size);
                }
                break;
#if defined(TIMER)
            case WM_TIMER:
                GetSelf(windowHandle)->OnTimer(int(wParam));
                break;
#endif // TIMER
            case WM_CHAR: {
                    const auto character = TCHAR(wParam);
                    GetSelf(windowHandle)->OnChar(character);
                }
                break;
            case WM_RBUTTONUP: {
                    const auto point = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    GetSelf(windowHandle)->OnRightButtonUp(point);
                }
                break;
            case WM_DESTROY:
                ::PostQuitMessage(0);
                break;
            default:
#if defined(USERMESSAGE)
                {
                    const auto self = GetSelf(windowHandle);
                    if (self == nullptr || !self->DispatchUserMessage(message, wParam, lParam))
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
        if (WNDCLASSEX existing{ .cbSize = sizeof(WNDCLASSEX) }; ::GetClassInfoEx(instanceHandle, windowClassName, &existing))
            return true;

        const WNDCLASSEX wcex = {
            .cbSize        = sizeof(WNDCLASSEX),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = windowProcedure,
            .hInstance     = instanceHandle,
            .hCursor       = ::LoadCursor(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
            .lpszClassName = windowClassName
        };
        return ::RegisterClassEx(&wcex) != 0;
    }

#if defined(USERMESSAGE)
    bool DispatchUserMessage(UINT userMessage, WPARAM wParam, LPARAM lParam)
    {
        if (ExistsUserMessage(userMessage)) {
            OnUserMessage(userMessage, wParam, lParam);
            return true;
        }
        return false;
    }

    bool ExistsUserMessage(UINT userMessage) const
    { return std::ranges::find(userMessages, userMessage) != userMessages.end(); }
#endif // USERMESSAGE
};

#if defined(TIMER)
class Timer final
{
    const HWND     windowHandle;
    const UINT_PTR timerId;

public:
    Timer(const Window& window, UINT_PTR timerId = 1U, UINT timeoutMilliseconds = 1000)
        : windowHandle(window.GetHandle()), timerId(::SetTimer(window.GetHandle(), timerId, timeoutMilliseconds, nullptr))
    {}

    ~Timer()
    {
        if (timerId != 0U)
            ::KillTimer(windowHandle, timerId);
    }

    Timer(const Timer&)            = delete;
    Timer& operator=(const Timer&) = delete;
};
#endif // TIMER

class Program
{
    Window* mainWindow = nullptr;

public:
    Program() = default;

    virtual ~Program() = default;

    Program(const Program&)            = delete;
    Program& operator=(const Program&) = delete;

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
        MSG message{};
#if defined(IDLE)
        for (;;) {
            if (::PeekMessage(&message, nullptr, 0U, 0U, PM_REMOVE)) {
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
