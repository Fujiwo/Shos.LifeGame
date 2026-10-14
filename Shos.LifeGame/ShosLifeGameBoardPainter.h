#pragma once

#include "ShosLifeGame.h"
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Shos::LifeGame::Win32 {

/// <summary>Paints the board as a monochrome bitmap.</summary>
/// <remarks>The bitmap and the memory device context are kept and reused, instead of being created every paint.</remarks>
class BoardPainter final
{
    HDC     memoryDeviceContextHandle = nullptr;
    HBITMAP bitmapHandle              = nullptr;
    SIZE    bitmapSize{};

public:
    BoardPainter() = default;

    ~BoardPainter()
    { Release(); }

    BoardPainter(const BoardPainter&)            = delete;
    BoardPainter& operator=(const BoardPainter&) = delete;

    /// <summary>Paints the board with its left-top at the position. One cell is one pixel.</summary>
    void Paint(HDC deviceContextHandle, const POINT& position, Board& board)
    {
        const auto size = SIZE{ board.GetSize().cx, board.GetSize().cy };
        if (bitmapHandle == nullptr || size.cx != bitmapSize.cx || size.cy != bitmapSize.cy) {
            Release();
            Create(deviceContextHandle, size);
        }

        // The bits of the board are copied without converting them.
        // The size must agree with the layout of BitCellSet (BitCellSet::GetRowByteNumber and ToIndex).
        ::SetBitmapBits(bitmapHandle, DWORD(BitCellSet::GetRowByteNumber(size.cx)) * DWORD(size.cy), board.GetBits());
        Paint(deviceContextHandle, position, size);
    }

private:
    /// <summary>Creates a monochrome bitmap: one bit per pixel, and each row is WORD-aligned (the same as BitCellSet).</summary>
    void Create(HDC deviceContextHandle, const SIZE& size)
    {
        memoryDeviceContextHandle = ::CreateCompatibleDC(deviceContextHandle);
        bitmapHandle              = ::CreateBitmap(size.cx, size.cy, 1U, 1U, nullptr);
        bitmapSize                = size;
    }

    void Release()
    {
        if (bitmapHandle != nullptr) {
            ::DeleteObject(bitmapHandle);
            bitmapHandle = nullptr;
        }
        if (memoryDeviceContextHandle != nullptr) {
            ::DeleteDC(memoryDeviceContextHandle);
            memoryDeviceContextHandle = nullptr;
        }
    }

    /// <summary>Copies the bitmap to the device context through the memory device context.</summary>
    /// <remarks>
    /// Bits of 0 (dead cells) are painted in the text color, and bits of 1 (alive cells) in the background color.
    /// The bitmap is selected only while it is copied, so that SetBitmapBits never changes a bitmap selected into a device context.
    /// </remarks>
    void Paint(HDC deviceContextHandle, const POINT& position, const SIZE& size)
    {
        const auto oldBitmapHandle = ::SelectObject(memoryDeviceContextHandle, bitmapHandle);
        ::BitBlt(deviceContextHandle, position.x, position.y, size.cx, size.cy, memoryDeviceContextHandle, 0, 0, SRCCOPY);
        ::SelectObject(memoryDeviceContextHandle, oldBitmapHandle);
    }
};

} // namespace Shos::LifeGame::Win32
