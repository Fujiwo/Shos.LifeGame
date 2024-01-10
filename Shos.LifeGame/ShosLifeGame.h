#pragma once

//#define USEBOOL // Boolean enabled
#define FAST    // Fast loops enabled
#define MT      // Multi-threading enabled
#define AREA    // Area enabled

#include <string>
#include <functional>
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
    { return Point(x + size.cx, y + size.cy); }

    Size operator -(const Point& point) const
    { return Size(x - point.x, y - point.y); }
};

struct Rect final
{
    Point leftTop;
    Size  size;

    Point RightBottom() const
    { return leftTop + size; }

#if defined(AREA) && defined(MT)
    Rect()
    {}

    static Rect Union(const Rect* rects, unsigned int count)
    {
        assert(count > 0);

        const auto rect0RightBottom = rects[0].RightBottom();
        auto left                   = rects[0].leftTop.x;
        auto top                    = rects[0].leftTop.y;
        auto right                  = rect0RightBottom.x;
        auto bottom                 = rect0RightBottom.y;

        for (auto index = 1U; index < count; index++) {
            const auto& rect            = rects[index];
            const auto  rectRightBottom = rect.RightBottom();
            left   = std::min(left  , rect.leftTop   .x);
            top    = std::min(top   , rect.leftTop   .y);
            right  = std::max(right , rectRightBottom.x);
            bottom = std::max(bottom, rectRightBottom.y);
        }

        return Rect(Point(left, top), Point(right, bottom));
    }
#endif // AREA && MT

    Rect(const Point& leftTop, const Size& size) : leftTop(leftTop), size(size)
    {}

    Rect(const Point& leftTop, const Point& rightBottom) : leftTop(leftTop), size(rightBottom - leftTop)
    {}

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
#if defined(AREA)
    static void ForEach(Integer minimum, Integer maximum, std::function<void(Integer, Integer, unsigned int)> action)
    {
        const auto               hardwareConcurrency = GetHardwareConcurrency();
        const Integer            size = maximum - minimum;
        std::vector<std::thread> threads;

        for (auto threadIndex = 0U; threadIndex < hardwareConcurrency; threadIndex++) {
            const auto index = threadIndex;
            const auto begin = minimum + size * index / hardwareConcurrency;
            const auto end   = minimum + (index == hardwareConcurrency - 1 ? size
                                                                           : size * (index + 1) / hardwareConcurrency);
            threads.emplace_back([=]() { action(begin, end, threadIndex); });
        }

        for (auto& thread : threads)
            thread.join();
    }
#else // AREA
    static void ForEach(Integer minimum, Integer maximum, std::function<void(Integer, Integer)> action)
    {
        const auto               hardwareConcurrency = GetHardwareConcurrency();
        const Integer            size = maximum - minimum;
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
#endif // AREA
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
    std::vector<Pattern> patterns;

public:
    size_t GetSize() const
    { return patterns.size(); }

    PatternSet()
    {
        const auto folderName = _T(".\\CellData");
        const auto extension5  = ".lif";
        ;
        std::vector<tstring> filePaths;
        Shos::File::GetFilePaths(folderName, filePaths, extension5);

        std::vector<Pattern> patterns5;
        Utility::Map<std::vector<tstring>, std::vector<Pattern>, tstring, Pattern>(filePaths, patterns5, [&](tstring filePath) { return Read(filePath); });

        patterns.clear();
        std::copy(patterns5.begin(), patterns5.end(), std::back_inserter(patterns));

        const auto extensionRle = ".rle";
        Shos::File::GetFilePaths(folderName, filePaths, extensionRle);

        std::vector<Pattern> patternsRle;
        Utility::Map<std::vector<tstring>, std::vector<Pattern>, tstring, Pattern>(filePaths, patternsRle, [&](tstring filePath) { return ReadRle(filePath); });
        std::copy(patternsRle.begin(), patternsRle.end(), std::back_inserter(patterns));
    }

    const Pattern& operator[](size_t index) const
    { return patterns[index]; }

private:
    static Pattern Read(tstring filePath)
    {
        std::vector<std::string> lines;
        Shos::File::Read(filePath, lines);

        std::vector<std::string> trimedLines;
        Utility::Map<std::vector<std::string>, std::vector<std::string>, std::string, std::string>(lines, trimedLines, [](std::string text) { return String::Trim(text); });

        std::vector<std::string> filteredLines;
        Utility::Filter<std::vector<std::string>, std::string>(trimedLines, filteredLines, [](const std::string& text) { return !text.starts_with('#'); });

        if (filteredLines.size() == 0)
            return Pattern(_T(""), "", 0U);

        const auto width    = Utility::Maximum<std::vector<std::string>, std::string, UnsignedInteger>(filteredLines, [](const std::string& text) { return UnsignedInteger(text.length()); });

        std::vector<std::string> ajustedLines;
        Utility::Map<std::vector<std::string>, std::vector<std::string>, std::string, std::string>(filteredLines, ajustedLines, [&](std::string text) { return Adjust(text, width); });

        std::string pattern = Utility::Connect(ajustedLines);

        return Pattern(Shos::File::GetStem(filePath), pattern, width);
    }

    static Pattern ReadRle(tstring filePath)
    {
        std::vector<std::string> lines;
        Shos::File::Read(filePath, lines);

        std::vector<std::string> trimedLines;
        Utility::Map<std::vector<std::string>, std::vector<std::string>, std::string, std::string>(lines, trimedLines, [](std::string text) { return String::Trim(text); });

        std::vector<std::string> filteredLines;
        Utility::Filter<std::vector<std::string>, std::string>(trimedLines, filteredLines, [](const std::string& text) { return !text.empty() && !text.starts_with('#'); });

        if (filteredLines.size() == 0)
            return Pattern(_T(""), "", 0U);

        const auto rule = filteredLines[0];
        auto [width, height] = GetRleSize(rule);

        std::vector<std::string> patternLines { filteredLines.begin() + 1, filteredLines.end() };
        //std::vector<std::string> patternLines;
        //std::copy(filteredLines.begin() + 1, filteredLines.end(), patternLines.begin());
        ////{ &filteredLines[1], filteredLines.end() };

        std::string pattern = Utility::Connect(patternLines);
        pattern = RleToPattern(pattern, width, height);

        return Pattern(Shos::File::GetStem(filePath), pattern, width);

        //const auto width = Utility::Maximum<std::vector<std::string>, std::string, UnsignedInteger>(filteredLines, [](const std::string& text) { return UnsignedInteger(text.length()); });

        //std::vector<std::string> ajustedLines;
        //Utility::Map<std::vector<std::string>, std::vector<std::string>, std::string, std::string>(filteredLines, ajustedLines, [&](std::string text) { return Adjust(text, width); });

        //std::string pattern = Utility::Connect(ajustedLines);

        //return Pattern(Shos::File::GetStem(filePath), pattern, width);
    }

    static std::string RleToPattern(std::string rlePattern, UnsignedInteger width, UnsignedInteger height)
    {
        std::string pattern    ;
        std::string partPattern;
        size_t      count = 0U;
        for (size_t index = 0U; index < rlePattern.length(); index++) {
            const auto  character = std::tolower(rlePattern[index]);
            if (std::isdigit(character)) {
                count = GetRleCount(rlePattern, index);
            } else {
                switch (character) {
                    case 'b':
                        partPattern.append(count == 0U ? 1U : count, '.');
                        break;
                    case 'o':
                        partPattern.append(count == 0U ? 1U : count, '*');
                        break;
                    case '$':
                        AppendPartPattern(partPattern, width, pattern);
                        partPattern.clear();
                        break;
                    case '!':
                        AppendPartPattern(partPattern, width, pattern);
                        return pattern;
                }
                count = 0U;
            }
        }
        return pattern;
    }

    static void AppendPartPattern(std::string& partPattern, UnsignedInteger width, std::string& pattern)
    {
        if      (partPattern.length() < width)
            pattern.append(Adjust(partPattern, width));
        else if (partPattern.length() > width)
            pattern.append(partPattern.substr(0, width));
        else
            pattern.append(partPattern);
    }

    static size_t GetRleCount(std::string rlePattern, size_t& index)
    {
        size_t count  = 0U;
        size_t number = 0U;
        for (; index < rlePattern.length(); number++) {
            const auto character = rlePattern[index + number];
            if (std::isdigit(character)) {
                count *= 10;
                count += character - '0';
            } else {
                break;
            }
        }
        if (number > 0U)
            index += number - 1;
        return count;
    }

    static std::tuple<UnsignedInteger, UnsignedInteger> GetRleSize(std::string rule)
    {
        rule.erase(std::remove_if(rule.begin(), rule.end(), ::isspace), rule.end());

        const auto tokens = String::Split(rule, ',');
        std::vector<std::string> trimedTokens;

        UnsignedInteger width  = 0U;
        UnsignedInteger height = 0U;

        try {
            for (const auto& token : tokens) {
                const auto lowerToken = String::ToLower(token);
                if      (lowerToken.starts_with("x="))
                    width  = std::stoi(token.substr(2));
                else if (lowerToken.starts_with("y="))
                    height = std::stoi(token.substr(2));
            }
        } catch (const std::exception&) {
        }
        return { width, height };
    }

    static std::string Adjust(std::string text, UnsignedInteger width)
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

#if defined(AREA)
    Rect            area;
#endif // AREA

public:
    Size GetSize() const
    { return size; }

    Rect GetArea() const
#if defined(AREA)
    { return area; }

    static Rect GetDefaultArea(const Rect& rect)
    { return Rect(Point(rect.leftTop.x + std::max(0, rect.size.cx / 2 - 1), rect.leftTop.y + std::max(0, rect.size.cy / 2 - 1)), Size(std::min(rect.size.cx, 3), std::min(rect.size.cy, 3))); }

#if defined(MT)
    void SetArea(const Rect& newArea)
    { area = newArea; }
#endif // MT

#else // AREA
    { return GetRect(); }
#endif // AREA

#if defined(AREA)
#endif // AREA

    UnitInteger* GetBits() const
    { return cells; }

    /// <remarks>size.cx must be a multiple of 8.</remarks>
    BitCellSet(const Size& size) : size(size)
#if defined(AREA)
        , area(GetDefaultArea(Rect(Point(), size)))
#endif // AREA
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
#if defined(AREA)
        std::tuple<UnsignedInteger, Byte> bitIndex;
        if (!ToIndex(point, bitIndex))
            return;

        const auto [index, bit] = bitIndex;
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
        std::tuple<UnsignedInteger, Byte> bitIndex;
        if (!ToIndex(point, bitIndex))
            return;

        const auto [index, bit] = bitIndex;
        value ? (cells[index] |= 1 << bit)
              : (cells[index] &= ~(1 << bit));
    }

    void Clear()
    {
        ::memset(cells, 0, GetUnitNumber() * (sizeof(UnitInteger) / sizeof(Byte)));
#if defined(AREA)
        area = GetDefaultArea(GetRect());
#endif // AREA
    }

    //void CopyTo(BitCellSet& bitCellSet) const
    //{
    //    assert(size == bitCellSet.size);
    //    ::memcpy(bitCellSet.cells, cells, GetUnitNumber() * (sizeof(UnitInteger) / sizeof(Byte)));
    //}

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

        //const auto left            = std::max(std::min(area.leftTop.x   , point.x - 1    ), rect.leftTop.x);
        //const auto top             = std::max(std::min(area.leftTop.y   , point.y - 1    ), rect.leftTop.y);
        //const auto right           = std::min(std::max(areaRightBottom.x, point.x + 1 + 1), rightBottom.x );
        //const auto bottom          = std::min(std::max(areaRightBottom.y, point.y + 1 + 1), rightBottom.y );

        //area.leftTop.x = left         ;
        //area.leftTop.y = top          ;
        //area.size.cx   = right  - left;
        //area.size.cy   = bottom - top ;

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
        assert(value == minimum || value == maximum - 1 || areaMinimum < value && value < areaMaximum - 1);
#endif // _DEBUG
    }

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

#if defined(AREA)
    Rect            area;
#endif // AREA

public:
    Size GetSize() const
    { return size; }

    Rect GetArea() const
#if defined(AREA)
    { return area; }
#if defined(MT)
    void SetArea(const Rect& newArea)
    { area = newArea; }
#endif // MT
#else // AREA
    { return GetRect(); }
#endif // AREA

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
#if defined(AREA)
        , area(BitCellSet::GetDefaultArea(Rect(Point(), size)))
#endif // AREA
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
        Utility::ForEach(Rect(startPoint, patternSize), [&](const Point& point) { Set(point, pattern[patternIndex++]); });
#endif // FAST

        return true;
    }

#if !defined(FAST)
    void ForEach(std::function<void(const Point&)> action
#if defined(AREA)
                 , bool areaOnly
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
    { return GetRect().IsIn(point) ? cells[point.y][point.x] : false; }

    void Set(const Point& point, bool value)
    {
        SetOnly(point, value);
#if defined(AREA)
        if (value)
            area = BitCellSet::Union(area, GetRect(), point);
#endif // AREA
    }

    void SetOnly(const Point& point, bool value)
    { cells[point.y][point.x] = value; }

private:
    void Initialize()
    {
        cells = new bool*[size.cy];
        for (auto y = 0; y < size.cy; y++)
            cells[y] = new bool[size.cx];

        Clear();
    }

    void Clear()
    {
        for (auto y = 0; y < size.cy; y++)
            ::memset(cells[y], 0, sizeof(bool) * size.cx);

#if defined(AREA)
        area = Rect(Point(std::min(0, size.cx / 2 - 1), std::min(0, size.cy / 2 - 1)), Size(std::min(size.cx, 3), std::min(size.cy, 3)));
#endif // AREA
    }

#if defined(AREA)
public:
#endif // AREA
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
    void ForEach(std::function<void(const Point&)> action, bool areaOnly)
    {
#if defined(AREA)
        Utility::ForEach(areaOnly ? GetArea() : GetRect(), action);
#else // AREA
        Utility::ForEach(GetRect(), action);
#endif // AREA
    }
#endif !FAST

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
#if defined(AREA) && defined(MT)
    Rect*              areas    ;
    const unsigned int hardwareConcurrency;
#endif // AREA && MT

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
#if defined(AREA) && defined(MT)
        , areas(nullptr), hardwareConcurrency(ThreadUtility::GetHardwareConcurrency())
#endif // AREA && MT
    { Initialize(true); }

    ~Game()
    {
#if defined(AREA) && defined(MT)
        delete[] areas;
#endif // AREA && MT
        delete subBoard;
        delete mainBoard;
    }

    void Next()
    {

#if defined(MT)
        //const auto size = mainBoard->GetSize();

        //ThreadUtility::ForEach(0, size.cy, [=](Integer minimum, Integer maximum) {
        //    NextPart(Point(0, minimum), Point(size.cx, maximum));
        //});

        const auto area            = mainBoard->GetArea();
        const auto areaRightBottom = area.RightBottom();

#if defined(AREA)
        ResetAreas();

        ThreadUtility::ForEach(area.leftTop.y, areaRightBottom.y, [=](Integer minimum, Integer maximum, unsigned int index) {
            NextPart(Point(area.leftTop.x, minimum), Point(areaRightBottom.x, maximum), areas[index]);
        });

        const auto newArea = Rect::Union(areas, hardwareConcurrency);
        subBoard->SetArea(newArea);

#else // AREA
        ThreadUtility::ForEach(area.leftTop.y, areaRightBottom.y, [=](Integer minimum, Integer maximum) {
            NextPart(Point(area.leftTop.x, minimum), Point(areaRightBottom.x, maximum));
        });
#endif // AREA

#elif defined(FAST)
        //NextPart(Point(), Point() + mainBoard->GetSize());
        const auto area = mainBoard->GetArea();
        NextPart(area.leftTop, area.RightBottom());
#else // FAST
        mainBoard->ForEach([&](const Point& point) {
            const auto aliveNeighborCount = mainBoard->GetAliveNeighborCount(point);
            const auto alive              = mainBoard->Get(point);
            subBoard->Set(point, aliveNeighborCount == 3 || (aliveNeighborCount == 2 && alive));
        });
#endif // FAST

#if defined(_DEBUG)
        Test(*mainBoard);
#endif // _DEBUG

        std::swap(mainBoard, subBoard);
        generation++;
    }

    void Reset(bool randomize)
    {
        Initialize(randomize);
        generation = 0UL;
        if (randomize)
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
        subBoard ->Set(patternSet[index]);
        patternIndex = index;
        return true;
    }

private:
    void Initialize(bool randomize)
    {
        if (randomize)
            Randomize();

#if defined(AREA) && defined(MT)
        delete[] areas;
        areas = new Rect[hardwareConcurrency];
        ResetAreas();
#endif // AREA && MT
    }

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
        }, false);
#endif // FAST
    }

#if defined(AREA) && defined(MT)
    void ResetAreas()
    {
        for (auto index = 0U; index < hardwareConcurrency; index++)
            areas[index] = mainBoard->GetArea();
    }
#endif // AREA && MT

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
