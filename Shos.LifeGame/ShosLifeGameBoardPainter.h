#pragma once

#include "ShosLifeGame.h"
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Shos::LifeGame::Win32 {

class BoardPainter final
{
public:
    static void Paint(HDC deviceContextHandle, const POINT& position, Board& board)
    {
        const auto bitmapHandle = CreateBitmap(board);
        Paint(deviceContextHandle, position, { board.GetSize().cx, board.GetSize().cy }, bitmapHandle);
        ::DeleteObject(bitmapHandle);
    }

private:
    static HBITMAP CreateBitmap(Board& board)
    {
        const auto   size   = board.GetSize();
        const BITMAP bitmap = {
            .bmType       = 0,
            .bmWidth      = size.cx,
            .bmHeight     = size.cy,
            .bmWidthBytes = LONG(BitCellSet::GetRowByteNumber(size.cx)),
            .bmPlanes     = 1,
            .bmBitsPixel  = 1,
            .bmBits       = board.GetBits()
        };
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

} // namespace Shos::LifeGame::Win32
