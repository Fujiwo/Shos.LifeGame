#pragma once

#define FAST
#define MT

#if !defined(FAST) || defined(MT)
#include <functional>
#endif // FAST
#include <tuple>
#if defined(MT)
#include <thread>
#endif // MT
#include <random>
#include <cstdlib>
#include <cstring>
#include <cassert>
using namespace std;

namespace Shos::LifeGame {

using Integer         = int          ;
using UnsignedInteger = unsigned int ;
using Byte            = unsigned char;
using UnitInteger     = Byte         ;

class Random
{
    random_device rd;
    mt19937       engine;

public:
    Random() : engine(rd())
    {}

    Integer Next()
    { return engine(); }
};

struct Size
{
    Integer cx;
    Integer cy;

    Size() : cx(0), cy(0)
    {}

    Size(Integer cx, Integer cy) : cx(cx), cy(cy)
    {}

    bool operator ==(const Size& size) const
    { return cx == size.cx && cy == size.cy; }
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
    { return x == point.x && y == point.y; }

    Point operator +(const Size& size) const
    { return { x + size.cx, y + size.cy }; }
};

struct Rect
{
    Point leftTop;
    Size  size;

    Point RightBottom() const
    { return leftTop + size; }

    bool IsIn(const Point& point) const
    {
        return IsIn(point.x, leftTop.x, size.cx) &&
               IsIn(point.y, leftTop.y, size.cy);
    }

private:
    template <typename T>
    bool IsIn(T value, T minimum, T size) const
    { return minimum <= value && value < minimum + size; }
};

#if !defined(FAST)
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
#endif // FAST

#if defined(MT)
class ThreadUtility
{
public:
    static void ForEach(Integer minimum, Integer maximum, function<void(Integer, Integer)> action)
    {
        const auto     hardwareConcurrency = GetHardwareConcurrency();
        const Integer  size = maximum - minimum;
        vector<thread> threads;

        for (auto threadIndex = 0U; threadIndex < hardwareConcurrency; threadIndex++) {
            const auto index = threadIndex;
            const auto begin = minimum + size * index / hardwareConcurrency;
            const auto end   = minimum + (index == hardwareConcurrency - 1 ? size
                                                                           : size * (index + 1) / hardwareConcurrency);
            threads.emplace_back([=]() { action(begin, end); });
        }

        for (auto& thread : threads)
            thread.join();
    }

private:
    static unsigned int GetHardwareConcurrency()
    {
        auto hardwareConcurrency = thread::hardware_concurrency();
        if (hardwareConcurrency == 0U)
            hardwareConcurrency = 1U;
        return hardwareConcurrency;
    }
};
#endif // MT

class Board
{
    const Size      size;
    UnsignedInteger unitNumberX;
    UnitInteger*    cells;

public:
    Size GetSize() const
    { return size; }

    UnitInteger* GetCells() const
    { return cells; }

    Board(const Size& size) : size(size)
    { Initialize(); }

    virtual ~Board()
    { delete[] cells; }

    void CopyTo(Board& board) const
    {
        assert(size == board.size);
        ::memcpy(board.cells, cells, GetUnitNumber() * (sizeof(UnitInteger) / sizeof(Byte)));
    }

#if !defined(FAST)
    void ForEach(function<void(const Point&)> action)
    { Utility::ForEach(GetRect(), action); }
#endif // FAST

    UnsignedInteger GetAliveNeighborCount(const Point& point) const
    {
#if defined(FAST)
        UnsignedInteger count = 0;
        for (Point neighborPoint = { point.x - 1, point.y - 1 }; neighborPoint.y <= point.y + 1; neighborPoint.y++) {
            for (neighborPoint.x = point.x - 1; neighborPoint.x <= point.x + 1; neighborPoint.x++) {
                if (neighborPoint == point)
                    continue;
                if (Get(neighborPoint))
                    count++;
            }
        }
        return count;
#else // FAST
        return Utility::Count(
            Rect(point + Size(-1, -1), Size(3, 3)),
            [&](const Point& neighborPoint) {
                return neighborPoint != point && Get(neighborPoint);
            }
        );
#endif // FAST
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
        ::memset(cells, 0, unitNumber * (sizeof(UnitInteger) / sizeof(Byte)));
    }

    void InitializeUnitNumberX()
    {
        unitNumberX = size.cx / sizeof(UnitInteger);
        if (size.cx % sizeof(UnitInteger) != 0)
            unitNumberX++;
    }

    UnsignedInteger GetUnitNumber() const
    { return unitNumberX * size.cy; }

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
    { return Rect(Point(), size); }
};

class Game
{
    Random random;
    Board* mainBoard;
    Board* subBoard;

public:
    const Board& GetBoard() const
    {
        return *mainBoard;
    }

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
        delete subBoard;
        delete mainBoard;
    }

    void Next()
    {
        mainBoard->CopyTo(*subBoard);

#if defined(MT)
        const auto size = mainBoard->GetSize();

        ThreadUtility::ForEach(0, size.cy, [=](Integer minimum, Integer maximum) {
            NextPart(Point(0, minimum), Point(size.cx, maximum));
        });
#elif defined(FAST)
        NextPart(Point(), Point() + mainBoard->GetSize());
#else // FAST
        mainBoard->ForEach([&](const Point& point) {
            const auto aliveNeighborCount = mainBoard->GetAliveNeighborCount(point);
            const auto alive              = mainBoard->Get(point);
            subBoard->Set(point, aliveNeighborCount == 3 || (aliveNeighborCount == 2 && alive));
        });
#endif // FAST

        swap(mainBoard, subBoard);
    }

private:
    void Initialize()
    {
#if defined(FAST)
        const auto size = mainBoard->GetSize();
        Point point;
        for (point.y = 0; point.y < size.cy; point.y++) {
            for (point.x = 0; point.x < size.cx; point.x++)
                mainBoard->Set(point, random.Next() % 2 == 0);
        }
#else // FAST
        mainBoard->ForEach([&](const Point& point) {
            mainBoard->Set(point, random.Next() % 2 == 0);
        });
#endif // FAST
    }

#if defined(FAST) || defined(MT)
    void NextPart(const Point& minimum, const Point& maximum)
    {
        Point point;
        for (point.y = minimum.y; point.y < maximum.y; point.y++) {
            for (point.x = minimum.x; point.x < maximum.x; point.x++) {
                const auto aliveNeighborCount = mainBoard->GetAliveNeighborCount(point);
                const auto alive = mainBoard->Get(point);
                subBoard->Set(point, aliveNeighborCount == 3 || (aliveNeighborCount == 2 && alive));
            }
        }
    }
#endif // FAST
};

} // namespace Shos::LifeGame
