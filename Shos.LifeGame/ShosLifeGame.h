#pragma once

// Define SHOS_LIFEGAME_CUSTOM_CONFIG to choose the following options outside this file (e.g. in tests).
#if !defined(SHOS_LIFEGAME_CUSTOM_CONFIG)
#define USEBITS // Bits enabled
#define FAST    // Fast loops enabled
#define MT      // Multi-threading enabled
#define AREA    // Area enabled
#endif // SHOS_LIFEGAME_CUSTOM_CONFIG

#include <string>
#include <functional>
#include <tuple>
#if defined(MT)
#include <concepts>
#include <thread>
#endif // MT

#include <cstdint>
#include <random>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <utility>
#include <vector>
#include <cassert>
#include <tchar.h>
#include "ShosHelper.h"
#if defined(_DEBUG)
#include "ShosDebug.h"
#endif // _DEBUG

namespace Shos::LifeGame {

using Integer         = int          ;
using UnsignedInteger = unsigned int ;
using Byte            = unsigned char;
using UnitInteger     = Byte         ;

class Random final
{
    std::random_device rd;
    std::mt19937       engine{ rd() };

public:
    std::uint32_t Next()
    { return engine(); }
};

struct Size final
{
    Integer cx = 0;
    Integer cy = 0;

    constexpr Size() = default;

    constexpr Size(Integer cx, Integer cy) : cx(cx), cy(cy)
    {}

    constexpr bool operator==(const Size&) const = default;
};

struct Point final
{
    Integer x = 0;
    Integer y = 0;

    constexpr Point() = default;

    constexpr Point(Integer x, Integer y) : x(x), y(y)
    {}

    // Not "= default": this is on the hot path (GetAliveNeighborCount), and the defaulted one was measurably slower with g++.
    constexpr bool operator==(const Point& point) const
    { return x == point.x && y == point.y; }

    constexpr Point operator+(const Size& size) const
    { return Point(x + size.cx, y + size.cy); }

    constexpr Size operator-(const Point& point) const
    { return Size(x - point.x, y - point.y); }
};

struct Rect final
{
    Point leftTop;
    Size  size;

    constexpr Point RightBottom() const
    { return leftTop + size; }

    constexpr Rect() = default;

    constexpr Rect(const Point& leftTop, const Size& size) : leftTop(leftTop), size(size)
    {}

    constexpr Rect(const Point& leftTop, const Point& rightBottom) : leftTop(leftTop), size(rightBottom - leftTop)
    {}

#if defined(AREA) && defined(MT)
    static Rect Union(std::span<const Rect> rects)
    {
        assert(!rects.empty());

        const auto rect0RightBottom = rects.front().RightBottom();
        auto left                   = rects.front().leftTop.x;
        auto top                    = rects.front().leftTop.y;
        auto right                  = rect0RightBottom.x;
        auto bottom                 = rect0RightBottom.y;

        for (const auto& rect : rects.subspan(1)) {
            const auto rectRightBottom = rect.RightBottom();
            left   = std::min(left  , rect.leftTop   .x);
            top    = std::min(top   , rect.leftTop   .y);
            right  = std::max(right , rectRightBottom.x);
            bottom = std::max(bottom, rectRightBottom.y);
        }

        return Rect(Point(left, top), Point(right, bottom));
    }
#endif // AREA && MT

    constexpr bool IsIn(const Point& point) const
    {
        return IsIn(point.x, leftTop.x, size.cx) &&
               IsIn(point.y, leftTop.y, size.cy);
    }

    constexpr bool IsEmpty() const
    { return size.cx <= 0 || size.cy <= 0; }

private:
    static constexpr bool IsIn(Integer value, Integer minimum, Integer size)
    { return minimum <= value && value < minimum + size; }
};

#if !defined(FAST)
// std::function is used on purpose: FAST replaces these generic loops with direct loops.
class Utility final
{
public:
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
};
#endif // FAST

#if defined(MT)
class ThreadUtility final
{
public:
    /// <summary>Splits [minimum, maximum) into ranges, and calls action(begin, end, threadIndex) for each range in its own thread.</summary>
    template <std::invocable<Integer, Integer, unsigned int> TAction>
    static void ForEach(Integer minimum, Integer maximum, const TAction& action)
    {
        const auto                hardwareConcurrency = GetHardwareConcurrency();
        const Integer             size                = maximum - minimum;
        std::vector<std::jthread> threads;

        for (auto threadIndex = 0U; threadIndex < hardwareConcurrency; threadIndex++) {
            const auto begin = minimum + Integer(size * threadIndex / hardwareConcurrency);
            const auto end   = minimum + (threadIndex == hardwareConcurrency - 1 ? size
                                                                                 : Integer(size * (threadIndex + 1) / hardwareConcurrency));
            threads.emplace_back([=, &action] { action(begin, end, threadIndex); });
        }
        // std::jthread joins on destruction.
    }

    static unsigned int GetHardwareConcurrency()
    { return std::max(std::thread::hardware_concurrency(), 1U); }
};
#endif // MT

class Pattern final
{
    static constexpr char alive = '*';
    tstring               name;
    std::string           pattern;
    UnsignedInteger       width;

public:
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

    Pattern(tstring name, std::string pattern, UnsignedInteger width) : name(std::move(name)), pattern(std::move(pattern)), width(width)
    {}

    bool operator[](size_t index) const
    { return pattern[index] == alive; }
};

class PatternSet final
{
    std::vector<Pattern> patterns;

public:
    size_t GetSize() const
    { return patterns.size(); }

    const Pattern& operator[](size_t index) const
    { return patterns[index]; }

    PatternSet(const tstring& folderName = _T(".\\CellData"))
    {
        Append(Pattern5  ::ReadFromFolder(folderName));
        Append(RlePattern::ReadFromFolder(folderName));
    }

private:
    void Append(std::vector<Pattern>&& newPatterns)
    { std::ranges::move(newPatterns, std::back_inserter(patterns)); }

    static std::vector<std::string> ReadTrimmedLines(const tstring& filePath)
    {
        auto lines = File::Read(filePath);
        for (auto& line : lines)
            line = String::Trim(line);
        return lines;
    }

    class Pattern5 final
    {
    public:
        static std::vector<Pattern> ReadFromFolder(const tstring& folderName)
        {
            std::vector<Pattern> patterns;
            for (const auto extension5 : { ".lif", ".life" }) {
                for (const auto& filePath : File::GetFilePaths(folderName, extension5))
                    patterns.push_back(Read(filePath));
            }
            return patterns;
        }

        static std::string Adjust(std::string text, size_t width)
        {
            constexpr auto character = '.';
            return String::Adjust(std::move(text), character, width);
        }

    private:
        /// <summary>Cell lines placed at "#P x y". Lines before the first "#P" are placed at (0, 0).</summary>
        struct Block
        {
            Point                    position;
            std::vector<std::string> lines;

            Integer GetWidth() const
            { return Integer(std::ranges::max(lines, {}, [](const std::string& line) { return line.length(); }).length()); }

            Integer GetHeight() const
            { return Integer(lines.size()); }
        };

        static Pattern Read(const tstring& filePath)
        {
            std::vector<Block> blocks;
            for (const auto& line : ReadTrimmedLines(filePath)) {
                if (line.starts_with("#P")) {
                    blocks.push_back(Block{ ReadPosition(line), {} });
                } else if (!line.starts_with('#') && !line.starts_with('!')) { // '!': comment of the Plaintext format
                    if (blocks.empty())
                        blocks.push_back(Block{ Point(), {} });
                    // An empty line is a row of dead cells, except at the beginning of a block.
                    if (!line.empty() || !blocks.back().lines.empty())
                        blocks.back().lines.push_back(line);
                }
            }
            for (auto& block : blocks) {
                while (!block.lines.empty() && block.lines.back().empty())
                    block.lines.pop_back();
            }
            std::erase_if(blocks, [](const Block& block) { return block.lines.empty(); });

            if (blocks.empty())
                return Pattern(_T(""), "", 0U);

            auto left   = blocks.front().position.x;
            auto top    = blocks.front().position.y;
            auto right  = left;
            auto bottom = top;
            for (const auto& block : blocks) {
                left   = std::min(left  , block.position.x);
                top    = std::min(top   , block.position.y);
                right  = std::max(right , block.position.x + block.GetWidth ());
                bottom = std::max(bottom, block.position.y + block.GetHeight());
            }

            const auto  width = UnsignedInteger(right - left);
            std::string pattern(size_t(width) * size_t(bottom - top), '.');
            for (const auto& block : blocks) {
                for (Integer y = 0; y < block.GetHeight(); y++) {
                    const auto& line = block.lines[size_t(y)];
                    for (Integer x = 0; x < Integer(line.length()); x++) {
                        if (line[size_t(x)] == '*')
                            pattern[size_t(block.position.y - top + y) * width + size_t(block.position.x - left + x)] = '*';
                    }
                }
            }

            return Pattern(File::GetStem(filePath), std::move(pattern), width);
        }

        /// <param name="line">"#P x y"</param>
        static Point ReadPosition(const std::string& line)
        {
            std::istringstream stream(line.substr(2));
            Point              position;
            if (!(stream >> position.x >> position.y))
                return Point();
            return position;
        }
    };

    class RlePattern final
    {
    public:
        static std::vector<Pattern> ReadFromFolder(const tstring& folderName)
        {
            std::vector<Pattern> patterns;
            for (const auto& filePath : File::GetFilePaths(folderName, ".rle"))
                patterns.push_back(Read(filePath));
            return patterns;
        }

    private:
        static Pattern Read(const tstring& filePath)
        {
            auto lines = ReadTrimmedLines(filePath);
            std::erase_if(lines, [](const std::string& text) { return text.empty() || text.starts_with('#'); });

            if (lines.empty())
                return Pattern(_T(""), "", 0U);

            const auto [width, height] = GetRleSize(lines.front());

            std::string rlePattern;
            for (const auto& line : lines | std::views::drop(1))
                rlePattern += line;

            return Pattern(File::GetStem(filePath), RleToPattern(rlePattern, width, height), width);
        }

        static std::string RleToPattern(std::string_view rlePattern, UnsignedInteger width, UnsignedInteger height)
        {
            std::string pattern;
            std::string partPattern;
            size_t      count = 0U;
            for (size_t index = 0U; index < rlePattern.length(); index++) {
                const auto character = std::tolower(static_cast<unsigned char>(rlePattern[index]));
                if (std::isdigit(character)) {
                    count = GetRleCount(rlePattern, index);
                } else {
                    const auto repeat = count == 0U ? 1U : count;
                    switch (character) {
                    case 'b':
                        partPattern.append(repeat, '.');
                        break;
                    case 'o':
                        partPattern.append(repeat, '*');
                        break;
                    case '$': // "n$" ends the line and adds (n - 1) empty lines.
                        for (size_t line = 0U; line < repeat; line++) {
                            AppendPartPattern(partPattern, width, pattern);
                            partPattern.clear();
                        }
                        break;
                    case '!':
                        AppendPartPattern(partPattern, width, pattern);
                        PadEmptyLines(width, height, pattern);
                        return pattern;
                    }
                    count = 0U;
                }
            }
            if (!partPattern.empty()) // Without the terminator '!'
                AppendPartPattern(partPattern, width, pattern);
            PadEmptyLines(width, height, pattern);
            return pattern;
        }

        /// <summary>Trailing empty lines may be omitted in RLE.</summary>
        static void PadEmptyLines(UnsignedInteger width, UnsignedInteger height, std::string& pattern)
        {
            const auto length = size_t(width) * size_t(height);
            if (pattern.length() < length)
                pattern.append(length - pattern.length(), '.');
        }

        static void AppendPartPattern(const std::string& partPattern, UnsignedInteger width, std::string& pattern)
        {
            if (partPattern.length() < width)
                pattern.append(Pattern5::Adjust(partPattern, width));
            else
                pattern.append(partPattern, 0, width);
        }

        static size_t GetRleCount(std::string_view rlePattern, size_t& index)
        {
            size_t count  = 0U;
            size_t number = 0U;
            for (; index + number < rlePattern.length(); number++) {
                const auto character = rlePattern[index + number];
                if (!std::isdigit(static_cast<unsigned char>(character)))
                    break;
                count = count * 10 + size_t(character - '0');
            }
            if (number > 0U)
                index += number - 1;
            return count;
        }

        static std::tuple<UnsignedInteger, UnsignedInteger> GetRleSize(std::string rule)
        {
            std::erase_if(rule, [](unsigned char character) { return std::isspace(character); });

            UnsignedInteger width  = 0U;
            UnsignedInteger height = 0U;

            try {
                for (const auto& token : String::Split(rule, ',')) {
                    const auto lowerToken = String::ToLower(token);
                    if (lowerToken.starts_with("x="))
                        width = UnsignedInteger(std::stoi(token.substr(2)));
                    else if (lowerToken.starts_with("y="))
                        height = UnsignedInteger(std::stoi(token.substr(2)));
                }
            } catch (const std::exception&) {
            }
            return { width, height };
        }
    };
};

class BitCellSet
{
    const Size                     size;
    const UnsignedInteger          unitNumberX;
    std::unique_ptr<UnitInteger[]> cells;

#if defined(AREA)
    Rect                           area;
#endif // AREA

public:
    Size GetSize() const
    { return size; }

    Rect GetArea() const
#if defined(AREA)
    { return area; }

    static Rect GetDefaultArea(const Rect& rect)
    { return Rect(Point(rect.leftTop.x + std::max(0, rect.size.cx / 2 - 1), rect.leftTop.y + std::max(0, rect.size.cy / 2 - 1)), Size(std::min(rect.size.cx, 3), std::min(rect.size.cy, 3))); }

    /// <summary>An area that contains no cells. Union with a point gives the area around the point.</summary>
    static Rect GetEmptyArea(const Size& size)
    { return Rect(Point(size.cx, size.cy), Point(0, 0)); }

    void SetArea(const Rect& newArea)
    { area = newArea; }

#else // AREA
    { return GetRect(); }
#endif // AREA

    /// <summary>Bits in the layout of a monochrome bitmap (see GetRowByteNumber).</summary>
    UnitInteger* GetBits() const
    { return cells.get(); }

    /// <summary>Bytes per row of the bits, which is WORD-aligned like a monochrome bitmap.</summary>
    static constexpr UnsignedInteger GetRowByteNumber(Integer width)
    { return UnsignedInteger((width + 15) / 16 * 2); }

    BitCellSet(const Size& size)
        : size(size), unitNumberX(GetRowByteNumber(size.cx)), cells(std::make_unique<UnitInteger[]>(GetUnitNumber()))
#if defined(AREA)
        , area(GetDefaultArea(GetRect()))
#endif // AREA
    { static_assert(sizeof(UnitInteger) == sizeof(Byte)); }

    virtual ~BitCellSet() = default;

    BitCellSet(const BitCellSet&)            = delete;
    BitCellSet& operator=(const BitCellSet&) = delete;

    bool Get(const Point& point) const
    {
        UnsignedInteger index;
        Byte            bit;
        if (!ToIndex(point, index, bit))
            return false;

        return (cells[index] & (1 << bit)) != 0;
    }

    void Set(const Point& point, bool value)
    {
#if defined(AREA)
        UnsignedInteger index;
        Byte            bit;
        if (!ToIndex(point, index, bit))
            return;

        if (value) {
            cells[index] |= 1 << bit;
            area = Union(area, GetRect(), point);
#if defined(_DEBUG)
            const auto rightBottom     = GetRect().RightBottom();
            const auto areaRightBottom = area     .RightBottom();
            assert(point.x == GetRect().leftTop.x || point.x == rightBottom.x - 1 || (area.leftTop.x < point.x && point.x < areaRightBottom.x - 1));
            assert(point.y == GetRect().leftTop.y || point.y == rightBottom.y - 1 || (area.leftTop.y < point.y && point.y < areaRightBottom.y - 1));
#endif // _DEBUG
        } else {
            cells[index] &= ~(1 << bit);
        }
#else // AREA
        SetOnly(point, value);
#endif // AREA
    }

    void SetOnly(const Point& point, bool value)
    {
        UnsignedInteger index;
        Byte            bit;
        if (!ToIndex(point, index, bit))
            return;

        value ? (cells[index] |= 1 << bit)
              : (cells[index] &= ~(1 << bit));
    }

    void Clear()
    {
        std::fill_n(cells.get(), GetUnitNumber(), UnitInteger(0));
#if defined(AREA)
        area = GetDefaultArea(GetRect());
#endif // AREA
    }

#if !defined(FAST)
    void ForEach(std::function<void(const Point&)> action, bool areaOnly)
    { Utility::ForEach(areaOnly ? GetArea() : GetRect(), action); }
#endif // FAST

#if defined(AREA)
    static Rect Union(const Rect& area, const Rect& rect, const Point& point)
    {
        const auto rightBottom     = rect.RightBottom();
        const auto areaRightBottom = area.RightBottom();

        auto left                  = area.leftTop    .x;
        auto right                 = areaRightBottom .x;
        Union(left, right, rect.leftTop.x, rightBottom.x, point.x);

        auto top                   = area.leftTop   .y;
        auto bottom                = areaRightBottom.y;
        Union(top, bottom, rect.leftTop.y, rightBottom.y, point.y);

#if defined(_DEBUG)
        assert(point.x == rect.leftTop.x || point.x == rect.RightBottom().x - 1 || (left < point.x && point.x < right  - 1));
        assert(point.y == rect.leftTop.y || point.y == rect.RightBottom().y - 1 || (top  < point.y && point.y < bottom - 1));

        const auto result            = Rect(Point(left, top), Point(right, bottom));
        const auto rectRightBottom   = rect  .RightBottom();
        const auto resultRightBottom = result.RightBottom();
        assert(point.x == rect.leftTop.x || point.x == rectRightBottom.x - 1 || (result.leftTop.x < point.x && point.x < resultRightBottom.x - 1));
        assert(point.y == rect.leftTop.y || point.y == rectRightBottom.y - 1 || (result.leftTop.y < point.y && point.y < resultRightBottom.y - 1));
#endif // _DEBUG

        return Rect(Point(left, top), Point(right, bottom));
    }
#endif // AREA

#if !defined(AREA) || !defined(MT)
protected:
#endif // !AREA || !MT
    Rect GetRect() const
    { return Rect(Point(), size); }

private:
    static void Union(Integer& areaMinimum, Integer& areaMaximum, Integer minimum, Integer maximum, Integer value)
    {
        areaMinimum = std::max(std::min(areaMinimum, value - 1    ), minimum);
        areaMaximum = std::min(std::max(areaMaximum, value + 1 + 1), maximum);
#if defined(_DEBUG)
        assert(value == minimum || value == maximum - 1 || (areaMinimum < value && value < areaMaximum - 1));
#endif // _DEBUG
    }

    UnsignedInteger GetUnitNumber() const
    { return unitNumberX * UnsignedInteger(size.cy); }

    /// <returns>false if the point is outside.</returns>
    /// <remarks>Out parameters, not std::optional: this is on the hot path, and std::optional made it measurably slower.</remarks>
    bool ToIndex(const Point& point, UnsignedInteger& index, Byte& bit) const
    {
        if (!GetRect().IsIn(point))
            return false;

        // The leftmost pixel is the most significant bit in a monochrome bitmap.
        constexpr auto bitNumber = sizeof(UnitInteger) * 8;
        index                    = UnsignedInteger(unitNumberX * point.y + point.x / bitNumber);
        bit                      = Byte(bitNumber - 1 - point.x % bitNumber);
        return true;
    }
};

#if !defined(USEBITS)
class Board final
{
    const Size                          size;
    std::vector<std::unique_ptr<bool[]>> cells; // cells[y][x]
    BitCellSet                          bitCellSet;

#if defined(AREA)
    Rect                                area;
#endif // AREA

public:
    Size GetSize() const
    { return size; }

    Rect GetArea() const
#if defined(AREA)
    { return area; }

    void SetArea(const Rect& newArea)
    { area = newArea; }
#else // AREA
    { return GetRect(); }
#endif // AREA

    UnitInteger* GetBits()
    {
        bitCellSet.Clear();

#if defined(FAST)
        for (auto point = Point{ 0, 0 }; point.y < size.cy; point.y++) {
            for (point.x = 0; point.x < size.cx; point.x++) {
                if (Get(point))
                    bitCellSet.SetOnly(point, true);
            }
        }
#else // FAST
        Utility::ForEach(Rect{ Point {0, 0}, size }, [&](const Point& point) { bitCellSet.SetOnly(point, Get(point)); });
#endif // FAST

        return bitCellSet.GetBits();
    }

    Board(const Size& size) : size(size), bitCellSet(size)
#if defined(AREA)
        , area(BitCellSet::GetDefaultArea(GetRect()))
#endif // AREA
    {
        for (auto y = 0; y < size.cy; y++)
            cells.push_back(std::make_unique<bool[]>(size_t(size.cx)));
    }

    Board(const Board&)            = delete;
    Board& operator=(const Board&) = delete;

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
        Utility::ForEach(Rect(startPoint, patternSize), [&](const Point& point) { Set(point, pattern[patternIndex++]); });
#endif // FAST

        return true;
    }

#if !defined(FAST)
    void ForEach(std::function<void(const Point&)> action
#if defined(AREA)
                 , bool areaOnly = false
#endif // AREA
    )
    {
        Utility::ForEach(
#if defined(AREA)
            areaOnly ? area :
#endif // AREA
            GetRect(), action);
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
    { return GetRect().IsIn(point) ? cells[size_t(point.y)][point.x] : false; }

    void Set(const Point& point, bool value)
    {
        SetOnly(point, value);
#if defined(AREA)
        if (value)
            area = BitCellSet::Union(area, GetRect(), point);
#endif // AREA
    }

    void SetOnly(const Point& point, bool value)
    { cells[size_t(point.y)][point.x] = value; }

private:
    void Clear()
    {
        for (auto& row : cells)
            std::fill_n(row.get(), size.cx, false);

#if defined(AREA)
        area = BitCellSet::GetDefaultArea(GetRect());
#endif // AREA
    }

#if defined(AREA)
public:
#endif // AREA
    Rect GetRect() const
    { return Rect(Point(), size); }
};
#else // USEBITS
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
    void ForEach(std::function<void(const Point&)> action, bool areaOnly = false)
    {
#if defined(AREA)
        Utility::ForEach(areaOnly ? GetArea() : GetRect(), action);
#else // AREA
        Utility::ForEach(GetRect(), action);
#endif // AREA
    }
#endif // !FAST

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
    { return BitCellSet::Get(point); }

    void Set(const Point& point, bool value)
    { BitCellSet::Set(point, value); }
};
#endif // USEBITS

class Game final
{
    Random                 random;
    std::unique_ptr<Board> mainBoard;
    std::unique_ptr<Board> subBoard;
    unsigned long long     generation = 0ULL;
    PatternSet             patternSet;
    std::optional<size_t>  patternIndex;
#if defined(AREA) && defined(MT)
    std::vector<Rect>      areas; // One for each thread
#endif // AREA && MT

public:
    const Board& GetBoard() const
    { return *mainBoard; }

    Board& GetBoard()
    { return *mainBoard; }

    unsigned long long GetGeneration() const
    { return generation; }

    tstring GetPatternName() const
    { return patternIndex ? patternSet[*patternIndex].GetName() : tstring(); }

    Game(const Size& size) : mainBoard(std::make_unique<Board>(size)), subBoard(std::make_unique<Board>(size))
#if defined(AREA) && defined(MT)
        , areas(ThreadUtility::GetHardwareConcurrency())
#endif // AREA && MT
    { Randomize(); }

    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;

    void Next()
    {
#if defined(AREA)
        // The area of subBoard is computed again from the cells alive in the next generation, so that it can shrink.
        ClearOutside(*subBoard, mainBoard->GetArea());
        subBoard->SetArea(BitCellSet::GetEmptyArea(subBoard->GetSize()));
#endif // AREA

#if defined(MT)
        const auto area            = mainBoard->GetArea();
        const auto areaRightBottom = area.RightBottom();

#if defined(AREA)
        std::ranges::fill(areas, BitCellSet::GetEmptyArea(mainBoard->GetSize()));

        ThreadUtility::ForEach(area.leftTop.y, areaRightBottom.y, [=, this](Integer minimum, Integer maximum, unsigned int index) {
            NextPart(Point(area.leftTop.x, minimum), Point(areaRightBottom.x, maximum), areas[index]);
        });

        subBoard->SetArea(Rect::Union(areas));

#else // AREA
        ThreadUtility::ForEach(area.leftTop.y, areaRightBottom.y, [=, this](Integer minimum, Integer maximum, unsigned int) {
            NextPart(Point(area.leftTop.x, minimum), Point(areaRightBottom.x, maximum));
        });
#endif // AREA

#elif defined(FAST)
        const auto area = mainBoard->GetArea();
        NextPart(area.leftTop, area.RightBottom());
#else // FAST
        mainBoard->ForEach([&](const Point& point) {
            const auto aliveNeighborCount = mainBoard->GetAliveNeighborCount(point);
            const auto alive              = mainBoard->Get(point);
            subBoard->Set(point, aliveNeighborCount == 3 || (aliveNeighborCount == 2 && alive));
        });
#endif // FAST

#if defined(AREA)
        if (subBoard->GetArea().IsEmpty()) // No cells alive
            subBoard->SetArea(BitCellSet::GetDefaultArea(Rect(Point(), subBoard->GetSize())));
#endif // AREA

#if defined(_DEBUG)
        Test(*mainBoard);
#endif // _DEBUG

        std::swap(mainBoard, subBoard);
        generation++;
    }

    void Reset(bool randomize)
    {
        if (randomize) {
            Randomize();
            patternIndex.reset();
        }
        generation = 0ULL;
    }

    bool SetPattern(int index)
    {
        if (index < 0 || patternSet.GetSize() <= size_t(index)) {
            patternIndex.reset();
            return false;
        }
        patternIndex = size_t(index);
        mainBoard->Set(patternSet[*patternIndex]);
        subBoard ->Set(patternSet[*patternIndex]);
        return true;
    }

private:
    void Randomize()
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

#if defined(AREA)
    /// <summary>Kills the cells in the area of the board that are outside of the given area.</summary>
    /// <remarks>The next generation is computed only in the area of mainBoard, so that cells outside of it must be dead in subBoard.</remarks>
    static void ClearOutside(Board& board, const Rect& area)
    {
        const auto boardArea            = board.GetArea();
        const auto boardAreaRightBottom = boardArea.RightBottom();
        const auto areaRightBottom      = area.RightBottom();

        // Only the difference of the areas is visited.
        for (auto y = boardArea.leftTop.y; y < boardAreaRightBottom.y; y++) {
            if (y < area.leftTop.y || areaRightBottom.y <= y) {
                ClearRow(board, y, boardArea.leftTop.x, boardAreaRightBottom.x);
            } else {
                ClearRow(board, y, boardArea.leftTop.x, std::min(area.leftTop.x, boardAreaRightBottom.x));
                ClearRow(board, y, std::max(areaRightBottom.x, boardArea.leftTop.x), boardAreaRightBottom.x);
            }
        }
    }

    static void ClearRow(Board& board, Integer y, Integer minimumX, Integer maximumX)
    {
        for (auto x = minimumX; x < maximumX; x++)
            board.SetOnly(Point(x, y), false);
    }
#endif // AREA

#if defined(AREA) && defined(MT)
    void NextPart(const Point& minimum, const Point& maximum, Rect& area)
    {
        Point point;
        for (point.y = minimum.y; point.y < maximum.y; point.y++) {
            for (point.x = minimum.x; point.x < maximum.x; point.x++) {
                const auto aliveNeighborCount = mainBoard->GetAliveNeighborCount(point);
                const auto alive              = aliveNeighborCount == 3 || (aliveNeighborCount == 2 && mainBoard->Get(point));
                subBoard->SetOnly(point, alive);

                if (alive)
                    area = BitCellSet::Union(area, mainBoard->GetRect(), point);
            }
        }
    }
#elif defined(FAST) || defined(MT)
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

#if defined(_DEBUG)
    static void Test(const Board& board)
    {
        const auto size = board.GetSize();
        const auto area = board.GetArea();
        assert(area.size.cx > 0 && area.size.cy > 0);
        assert(area.leftTop.x >= 0 && area.leftTop.y >= 0 && area.RightBottom().x <= size.cx && area.RightBottom().y <= size.cy);

        for (auto y = 0; y < size.cy; y++) {
            for (auto x = 0; x < size.cx; x++) {
                if (!area.IsIn(Point(x, y)))
                    assert(!board.Get(Point(x, y)));
            }
        }
    }
#endif // _DEBUG
};

} // namespace Shos::LifeGame
