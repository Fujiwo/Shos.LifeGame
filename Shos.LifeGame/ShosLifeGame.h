#pragma once

#define FAST    // Fast loops enabled
#define MT      // Multi-threading enabled
//#define USEBOOL // Boolean enabled

#include <string>
#if !defined(FAST) || defined(MT)
#include <functional>
#endif // FAST
#include <tuple>
#if defined(MT)
#include <thread>
#endif // MT
#include <random>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <tchar.h>
#include "ShosFile.h"

namespace Shos::LifeGame {

using Integer         = int          ;
using UnsignedInteger = unsigned int ;
using Byte            = unsigned char;
using UnitInteger     = Byte         ;

class Random final
{
    std::random_device rd;
    std::mt19937       engine;

public:
    Random() : engine(rd())
    {}

    Integer Next()
    { return engine(); }
};

struct Size final
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

struct Point final
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

struct Rect final
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

class Utility final
{
public:
#if !defined(FAST)
    static UnsignedInteger Count(const Rect& rect, std::function<bool(const Point&)> isMatch)
    {
        UnsignedInteger count = 0;
        ForEach(rect, [&](const Point& point) {
            if (isMatch(point))
                count++;
        });
        return count;
    }

    static void ForEach(const Rect& rect, std::function<void(const Point&)> action)
    {
        const Point rightBottom = rect.RightBottom();

        for (auto index = rect.leftTop; index.y < rightBottom.y; index.y++) {
            for (index.x = rect.leftTop.x; index.x < rightBottom.x; index.x++)
                action(index);
        }
    }
#endif // FAST

    template <typename TCollection, typename TElement>
    static void ForEach(const TCollection& collection, std::function<void(const TElement&)> action)
    {
        for (const auto& element : collection)
            action(element);
    }

    template <typename TSourceCollection, typename TDestinationCollection, typename TSourceElement, typename TDestinationElement>
    static void Map(const TSourceCollection& source, TDestinationCollection& destination, std::function<TDestinationElement(const TSourceElement&)> map)
    {
        destination.clear();
        for (const auto& element : source)
            destination.push_back(map(element));
    }

    template <typename TCollection, typename TElement>
    static void Filter(const TCollection& source, TCollection& destination, std::function<bool(const TElement&)> isMatch)
    {
        destination.clear();
        for (const auto& element : source) {
            if (isMatch(element))
                destination.push_back(element);
        }
    }

    template <typename TCollection, typename TElement, typename TSizeType = size_t>
    static TSizeType Maximum(const TCollection& collection, std::function<TSizeType(const TElement&)> getValue)
    {
        assert(collection.size() > 0);

        std::vector<TSizeType> values;
        Map<TCollection, std::vector<TSizeType>, TElement, TSizeType>(collection, values, getValue);
        auto iterator = std::max_element(values.begin(), values.end());
        return *iterator;
    }


    template <typename TCollection>
    static std::string Connect(const TCollection& texts)
    {
        std::ostringstream stream;
        ForEach<TCollection, std::string>(texts, [&](const std::string& text) { stream << text; });
        return stream.str();
    }
};

#if defined(MT)
class ThreadUtility final
{
public:
    static void ForEach(Integer minimum, Integer maximum, std::function<void(Integer, Integer)> action)
    {
        const auto               hardwareConcurrency = GetHardwareConcurrency();
        const Integer            size                = maximum - minimum;
        std::vector<std::thread> threads;

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
        auto hardwareConcurrency = std::thread::hardware_concurrency();
        if (hardwareConcurrency == 0U)
            hardwareConcurrency = 1U;
        return hardwareConcurrency;
    }
};
#endif // MT

class Pattern final
{
//public:
//    enum class Type
//    {
//        Glider               ,
//        LightweightSpaceship ,
//        MiddleweightSpaceship,
//        HeavyweightSpaceship ,
//        Rpentomino           ,
//        GosperGliderGun      ,
//        SimkinGliderGun      ,
//        SwitchEngine         ,
//        PufferTrain          ,
//        Max                  ,
//        PatternCount
//    };

private:
    const char            alive = '*';
    const tstring         name;
    const std::string     pattern;
    const UnsignedInteger width;

public:
    //static Pattern Create(Type type)
    //{
    //    switch (type) {
    //    case Type::Glider:
    //        return Pattern(
    //            _T("Glider"),
    //            "***"
    //            "*.."
    //            ".*."
    //        , 3U);
    //    case Type::LightweightSpaceship:
    //        return Pattern(
    //            _T("LightweightSpaceship"),
    //            ".*..*"
    //            "*...."
    //            "*...*"
    //            "****."
    //        , 5U);
    //    case Type::MiddleweightSpaceship:
    //        return Pattern(
    //            _T("MiddleweightSpaceship"),
    //            "...*.."
    //            ".*...*"
    //            "*....."
    //            "*....*"
    //            "*****."
    //        , 6U);
    //    case Type::HeavyweightSpaceship:
    //        return Pattern(
    //            _T("HeavyweightSpaceship"),
    //            "...**.."
    //            ".*....*"
    //            "*......"
    //            "*.....*"
    //            "******."
    //        , 7U);
    //    case Type::Rpentomino:
    //        return Pattern(
    //            _T("Rpentomino"),
    //            ".**"
    //            "**."
    //            ".*."
    //        , 3U);
    //    case Type::GosperGliderGun:
    //        return Pattern(
    //            _T("GosperGliderGun"),
    //            "........................*..........."
    //            "......................*.*..........."
    //            "............**......**............**"
    //            "...........*...*....**............**"
    //            "**........*.....*...**.............."
    //            "**........*...*.**....*.*..........."
    //            "..........*.....*.......*..........."
    //            "...........*...*...................."
    //            "............**......................"
    //        , 36U);
    //    case Type::SimkinGliderGun:
    //        return Pattern(
    //            _T("SimkinGliderGun"),
    //            "**.....**........................"
    //            "**.....**........................"
    //            "................................."
    //            "....**..........................."
    //            "....**..........................."
    //            "................................."
    //            "................................."
    //            "................................."
    //            "................................."
    //            "......................**.**......"
    //            ".....................*.....*....."
    //            ".....................*......*..**"
    //            ".....................***...*...**"
    //            "..........................*......"
    //            "................................."
    //            "................................."
    //            "................................."
    //            "....................**..........."
    //            "....................*............"
    //            ".....................***........."
    //            ".......................*........."
    //            , 33U);
    //    case Type::SwitchEngine:
    //        return Pattern(
    //            _T("SwitchEngine"),
    //            "***............................................"
    //            "*.*............................................"
    //            "***............................................"
    //            "*.*............................................"
    //            "***............................................"
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "***..................*..................**....."
    //            ".*..*................*..*.................*...."
    //            ".....*..............*.*.*.................*...."
    //            "..*.*......................................****"
    //            "........................**....................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "***............................................"
    //            "*.*............................................"
    //            "***............................................"
    //            "..*............................................"
    //            "..*............................................"
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "..............................................."
    //            "**..................*.....................*...."
    //            "....................**..................*......"
    //            ".*.......................................*....."
    //            "..**.*.*.............***.................*....."
    //            "........................*......................"
    //            "....*.*...............**................*.***.."
    //            "..............................................."
    //            "...........................................*..."
    //        , 47U);
    //    case Type::PufferTrain:
    //        return Pattern(
    //            _T("PufferTrain"),
    //            "...*."
    //            "....*"
    //            "*...*"
    //            ".****"
    //            "....."
    //            "....."
    //            "....."
    //            "*...."
    //            ".**.."
    //            "..*.."
    //            "..*.."
    //            ".*..."
    //            "....."
    //            "....."
    //            "...*."
    //            "....*"
    //            "*...*"
    //            ".****"
    //            , 5U);
    //    case Type::Max:
    //        return Pattern(
    //            _T("Max"),
    //            "..................*........"
    //            ".................***......."
    //            "............***....**......"
    //            "...........*..***..*.**...."
    //            "..........*...*.*..*.*....."
    //            "..........*....*.*.*.*.**.."
    //            "............*....*.*...**.."
    //            "****.....*.*....*...*.***.."
    //            "*...**.*.***.**.........**."
    //            "*.....**.....*............."
    //            ".*..**.*..*..*.**.........."
    //            ".......*.*.*.*.*.*.....****"
    //            ".*..**.*..*..*..**.*.**...*"
    //            "*.....**...*.*.*...**.....*"
    //            "*...**.*.**..*..*..*.**..*."
    //            "****.....*.*.*.*.*.*......."
    //            "..........**.*..*..*.**..*."
    //            ".............*.....**.....*"
    //            ".**.........**.***.*.**...*"
    //            "..***.*...*....*.*.....****"
    //            "..**...*.*....*............"
    //            "..**.*.*.*.*....*.........."
    //            ".....*.*..*.*...*.........."
    //            "....**.*..***..*..........."
    //            "......**....***............"
    //            ".......***................."
    //            "........*.................."
    //            , 27U);
    //    default:
    //        assert(false);
    //        return Pattern(_T(""), "", 0U);
    //    }   
    //}

    Size GetSize() const
    {
        if (width == 0)
            return Size();

        auto size = Size{ Integer(width), Integer(pattern.length() / width) };
        if (pattern.length() % width != 0)
            size.cy++;
        return size;
    }

    const tstring& GetName() const
    { return name; }

    Pattern(const tstring& name, const std::string& pattern, UnsignedInteger width) : name(name), pattern(pattern), width(width)
    {}

    bool operator[](size_t index) const
    { return pattern[index] == alive; }

//    bool operator[](const Point& point) const
//    { return operator[](ToIndex(point)); }
//
//private:
//    size_t ToIndex(const Point& point) const
//    { return width * point.y + point.x; }
};

class PatternSet final
{
    std::vector<Pattern> paterns;

public:
    size_t GetSize() const
    { return paterns.size(); }

    PatternSet()
    {
        const auto folderName = _T(".\\CellData");
        const auto extension  = ".lif";

        std::vector<tstring> filePaths;
        Shos::File::GetFilePaths(folderName, filePaths, extension);

        Utility::Map<std::vector<tstring>, std::vector<Pattern>, tstring, Pattern>(filePaths, paterns, [&](tstring filePath) { return Read(filePath); });
    }

    const Pattern& operator[](size_t index) const
    { return paterns[index]; }

private:
    Pattern Read(tstring filePath)
    {
        std::vector<std::string> lines;
        Shos::File::Read(filePath, lines);

        std::vector<std::string> trimedLines;
        Utility::Map<std::vector<std::string>, std::vector<std::string>, std::string, std::string>(lines, trimedLines, [](std::string text) { return String::Trim(text); });

        std::vector<std::string> filteredLines;
        Utility::Filter<std::vector<std::string>, std::string>(trimedLines, filteredLines, [](const std::string& text) { return !text.empty() && !text.starts_with('#'); });

        if (filteredLines.size() == 0)
            return Pattern(_T(""), "", 0U);

        const auto width    = Utility::Maximum<std::vector<std::string>, std::string, UnsignedInteger>(filteredLines, [](const std::string& text) { return UnsignedInteger(text.length()); });

        std::vector<std::string> ajustedLines;
        Utility::Map<std::vector<std::string>, std::vector<std::string>, std::string, std::string>(filteredLines, ajustedLines, [&](std::string text) { return Adjust(text, width); });

        std::string pattern = Utility::Connect(ajustedLines);

        return Pattern(Shos::File::GetStem(filePath), pattern, width);
    }

    std::string Adjust(std::string text, UnsignedInteger width)
    {
        if (text.length() < width)
            text.append(width - text.length(), '.');
        return text;
    }
};

class BitCellSet
{
    const Size      size;
    UnsignedInteger unitNumberX;
    UnitInteger*    cells;

public:
    Size GetSize() const
    { return size; }

    UnitInteger* GetBits() const
    { return cells; }

    /// <remarks>size.cx must be a multiple of 8.</remarks>
    BitCellSet(const Size& size) : size(size)
    { Initialize(); }

    virtual ~BitCellSet()
    { delete[] cells; }

    bool Get(const Point& point) const
    {
        std::tuple<UnsignedInteger, Byte> bitIndex;
        if (!ToIndex(point, bitIndex))
            return false;

        const auto [index, bit] = bitIndex;
        return (cells[index] & (1 << bit)) != 0;
    }

    void Set(const Point& point, bool value)
    {
        std::tuple<UnsignedInteger, Byte> bitIndex;
        if (!ToIndex(point, bitIndex))
            return;

        const auto [index, bit] = bitIndex;
        value ? (cells[index] |= 1 << bit)
            : (cells[index] &= ~(1 << bit));
    }
    
    void Clear() const
    { ::memset(cells, 0, GetUnitNumber() * (sizeof(UnitInteger) / sizeof(Byte))); }

    //void CopyTo(BitCellSet& bitCellSet) const
    //{
    //    assert(size == bitCellSet.size);
    //    ::memcpy(bitCellSet.cells, cells, GetUnitNumber() * (sizeof(UnitInteger) / sizeof(Byte)));
    //}

#if !defined(FAST)
    void ForEach(std::function<void(const Point&)> action)
    {
        Utility::ForEach(GetRect(), action);
    }
#endif // FAST

protected:
    Rect GetRect() const
    { return Rect(Point(), size); }

private:
    void Initialize()
    {
        InitializeUnitNumberX();
        const auto unitNumber = GetUnitNumber();
        cells                 = new UnitInteger[unitNumber];
        Clear();
    }

    void InitializeUnitNumberX()
    {
        unitNumberX = size.cx / sizeof(UnitInteger);
        if (size.cx % sizeof(UnitInteger) != 0) {
            assert(false);
            unitNumberX++;
        }
    }

    UnsignedInteger GetUnitNumber() const
    { return unitNumberX * size.cy; }

    bool ToIndex(const Point& point, std::tuple<UnsignedInteger, Byte>& bitIndex) const
    {
        if (!GetRect().IsIn(point))
            return false;

        constexpr auto bitNumber = sizeof(UnitInteger) * 8;
        const auto index         = UnsignedInteger(unitNumberX * point.y + point.x / bitNumber);
        const auto bit           = Byte(point.x % bitNumber);
        bitIndex                 = { index, bit };
        return true;
    }
};

#if defined(USEBOOL)
class Board final
{
    const Size      size;
    bool**          cells;
    BitCellSet*     bitCellSet;

public:
    Size GetSize() const
    { return size; }

    UnitInteger* GetBits()
    {
        delete bitCellSet;
        bitCellSet = new BitCellSet(size);

#if defined(FAST)
        for (auto point = Point{ 0, 0 }; point.y < size.cy; point.y++) {
            for (point.x = 0; point.x < size.cx; point.x++)
                bitCellSet->Set(point, Get(point));
        }
#else // FAST
        Utility::ForEach(Rect{ Point {0, 0}, size }, [&](const Point& point) { bitCellSet->Set(point, Get(point)); });
#endif // FAST

        return bitCellSet->GetBits();
    }

    /// <remarks>size.cx must be a multiple of 8.</remarks>
    Board(const Size& size) : size(size), bitCellSet(nullptr)
    { Initialize(); }

    ~Board()
    {
        delete[] bitCellSet;
        delete[] cells     ;
    }

    bool Set(const Pattern& pattern)
    {
        const auto patternSize    = pattern.GetSize();
        const auto bitCellSetSize = GetSize();
        if (patternSize.cx > bitCellSetSize.cx || patternSize.cy > bitCellSetSize.cy)
            return false;

        Clear();

        const auto startPoint = Point{ (bitCellSetSize.cx - patternSize.cx) / 2, (bitCellSetSize.cy - patternSize.cy) / 2 };
        size_t     patternIndex = 0U;

#if defined(FAST)
        for (Point point = startPoint; point.y < startPoint.y + patternSize.cy; point.y++) {
            for (point.x = startPoint.x; point.x < startPoint.x + patternSize.cx; point.x++)
                Set(point, pattern[patternIndex++]);
        }
#else // FAST
        Utility::ForEach(Rect{ startPoint, patternSize }, [&](const Point& point) { Set(point, pattern[patternIndex++]); });
#endif // FAST

        return true;
    }

#if !defined(FAST)
    void ForEach(std::function<void(const Point&)> action)
    {
        Utility::ForEach(GetRect(), action);
    }
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
    { return GetRect().IsIn(point) ? cells[point.y][point.x] : false; }

    void Set(const Point& point, bool value)
    { cells[point.y][point.x] = value; }

private:
    void Initialize()
    {
        cells = new bool*[size.cy];
        for (auto y = 0; y < size.cy; y++)
            cells[y] = new bool[size.cx];

        Clear();
    }

    void Clear() const
    {
        for (auto y = 0; y < size.cy; y++)
            ::memset(cells[y], 0, sizeof(bool) * size.cx);
    }

    Rect GetRect() const
    { return Rect(Point(), size); }
};
#else // USEBOOL
class Board final : public BitCellSet
{
public:
    Board(const Size& size) : BitCellSet(size)
    {}

    bool Set(const Pattern& pattern)
    {
        const auto patternSize    = pattern.GetSize();
        const auto bitCellSetSize = GetSize();
        if (patternSize.cx > bitCellSetSize.cx || patternSize.cy > bitCellSetSize.cy)
            return false;

        Clear();

        const auto startPoint   = Point { (bitCellSetSize.cx - patternSize.cx) / 2, (bitCellSetSize.cy - patternSize.cy) / 2 };
        size_t     patternIndex = 0U;

#if defined(FAST)
        for (Point point = startPoint; point.y < startPoint.y + patternSize.cy; point.y++) {
            for (point.x = startPoint.x; point.x < startPoint.x + patternSize.cx; point.x++)
                Set(point, pattern[patternIndex++]);
        }
#else // FAST
        Utility::ForEach(Rect{ startPoint, patternSize }, [&](const Point& point) { Set(point, pattern[patternIndex++]); });
#endif // FAST

        return true;
    }

#if !defined(FAST)
    void ForEach(std::function<void(const Point&)> action)
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
    { return  BitCellSet::Get(point); }
    
    void Set(const Point& point, bool value)
    { BitCellSet::Set(point, value); }
};
#endif // USEBOOL

class Game final
{
    Random             random    ;
    Board*             mainBoard ;
    Board*             subBoard  ;
    unsigned long long generation;
    PatternSet         patternSet;
    int                patternIndex;

public:
    const Board& GetBoard() const
    { return *mainBoard; }

    Board& GetBoard()
    { return *mainBoard; }

    unsigned long long GetGeneration() const
    { return generation; }

    tstring GetPatternName() const
    { return 0 <= patternIndex && patternIndex < patternSet.GetSize() ? patternSet[patternIndex].GetName() : _T(""); }

    Game(const Size& size) : mainBoard(new Board(size)), subBoard(new Board(size)), generation(0UL), patternIndex(-1)
    { Initialize(); }

    ~Game()
    {
        delete subBoard;
        delete mainBoard;
    }

    void Next()
    {
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

        std::swap(mainBoard, subBoard);
        generation++;
    }

    void Reset()
    {
        Initialize();
        generation   = 0UL;
        patternIndex = -1;
    }

    //void Set(Pattern::Type patternType)
    //{ mainBoard->Set(Pattern::Create(patternType)); }

    bool SetPattern(int index)
    {
        if (index < 0 || patternSet.GetSize() <= index) {
            patternIndex = -1;
            return false;
        }
        mainBoard->Set(patternSet[index]);
        patternIndex = index;
        return true;
    }

private:
    void Initialize()
    {
        //setlocale(LC_CTYPE, "");
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
                subBoard->Set(point, aliveNeighborCount == 3 || (aliveNeighborCount == 2 && mainBoard->Get(point)));
            }
        }
    }
#endif // FAST
};

} // namespace Shos::LifeGame
