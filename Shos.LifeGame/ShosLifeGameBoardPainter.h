#pragma once

#include "ShosLifeGame.h"
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Shos::LifeGame::Win32 {

/// <summary>Paints the board as a monochrome bitmap.</summary>
class BoardPainter final
{
public:
    /// <summary>Paints the board with its left-top at the position. One cell is one pixel.</summary>
    static void Paint(HDC deviceContextHandle, const POINT& position, Board& board)
    {
        const auto bitmapHandle = CreateBitmap(board);
        Paint(deviceContextHandle, position, { board.GetSize().cx, board.GetSize().cy }, bitmapHandle);
        ::DeleteObject(bitmapHandle);
    }

private:
    /// <summary>Creates a bitmap from the bits of the board without converting them.</summary>
    /// <remarks>bmWidthBytes must agree with the layout of BitCellSet (BitCellSet::GetRowByteNumber and ToIndex).</remarks>
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

    /// <summary>Copies the bitmap to the device context through a memory device context.</summary>
    /// <remarks>Bits of 0 (dead cells) are painted in the text color, and bits of 1 (alive cells) in the background color.</remarks>
    static void Paint(HDC deviceContextHandle, const POINT& position, const SIZE& size, HBITMAP bitmapHandle)
    {
        const auto memoryDeviceContextHandle = ::CreateCompatibleDC(deviceContextHandle);
        ::SelectObject(memoryDeviceContextHandle, bitmapHandle);
        ::BitBlt(deviceContextHandle, position.x, position.y, size.cx, size.cy, memoryDeviceContextHandle, 0, 0, SRCCOPY);
        ::DeleteDC(memoryDeviceContextHandle);
    }
};

} // namespace Shos::LifeGame::Win32
