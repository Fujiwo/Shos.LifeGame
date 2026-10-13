// Regression tests for Shos.LifeGame.
//
// The optimization options (USEBITS / FAST / MT / AREA) can be chosen from outside
// by defining SHOS_LIFEGAME_CUSTOM_CONFIG, so that every combination can be tested
// with the same source. See run_regression_tests.sh.
//
// Tests marked as KnownBug document a defect reported in
// Reviews/2026-10-13_RepositoryReview.md. They are expected to fail (XFAIL).
// When the defect is fixed, the test passes (XPASS) and the run fails, so that
// the mark is removed and the test becomes a normal regression test.

#include "../Shos.LifeGame/ShosLifeGame.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace Shos::LifeGame::Test {

using namespace Shos::LifeGame;

// ---- Minimal test framework -------------------------------------------------

struct TestFailure
{
    std::string message;
};

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition))                                                                      \
            throw TestFailure{ std::string(__FILE__ ":") + std::to_string(__LINE__) +          \
                               ": CHECK(" #condition ") failed" };                             \
    } while (false)

#define CHECK_EQ(expected, actual)                                                             \
    do {                                                                                       \
        const auto& expected_ = (expected);                                                    \
        const auto& actual_   = (actual);                                                      \
        if (!(expected_ == actual_))                                                           \
            throw TestFailure{ std::string(__FILE__ ":") + std::to_string(__LINE__) +          \
                               ": CHECK_EQ(" #expected ", " #actual ")\n      expected: " +    \
                               ToText(expected_) + "\n      actual  : " + ToText(actual_) };   \
    } while (false)

enum class Expect { Pass, KnownBug };

struct TestCase
{
    const char*           name;
    Expect                expect;
    const char*           note;     // Review item for KnownBug
    std::function<void()> run;
};

// ---- Helpers ----------------------------------------------------------------

using Cell  = std::pair<Integer, Integer>; // (x, y)
using Cells = std::set<Cell>;

template <typename T>
std::string ToText(const T& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

std::string ToText(const std::string& value)
{ return "\"" + value + "\""; }

std::string ToText(const Cells& cells)
{
    std::string text = "{";
    for (const auto& [x, y] : cells)
        text += " (" + std::to_string(x) + "," + std::to_string(y) + ")";
    return text + " }";
}

std::string ToText(const std::vector<std::string>& rows)
{
    std::string text = "[";
    for (const auto& row : rows)
        text += " \"" + row + "\"";
    return text + " ]";
}

tstring ToTString(const std::filesystem::path& path)
{
#if defined(UNICODE) || defined(_UNICODE)
    return path.wstring();
#else // UNICODE
    return path.string();
#endif // UNICODE
}

Cells Shift(const Cells& cells, Integer dx, Integer dy)
{
    Cells result;
    for (const auto& [x, y] : cells)
        result.insert({ x + dx, y + dy });
    return result;
}

Pattern MakePattern(const Size& size, const Cells& cells)
{
    std::string text(size_t(size.cx * size.cy), '.');
    for (const auto& [x, y] : cells)
        text[size_t(y * size.cx + x)] = '*';
    return Pattern(_T("test"), text, UnsignedInteger(size.cx));
}

Cells GetAliveCells(const Board& board)
{
    Cells cells;
    const auto size = board.GetSize();
    for (Integer y = 0; y < size.cy; y++) {
        for (Integer x = 0; x < size.cx; x++) {
            if (board.Get(Point(x, y)))
                cells.insert({ x, y });
        }
    }
    return cells;
}

/// Places cells on a board of the same size as the game, so that the pattern is not shifted.
void Place(Game& game, const Cells& cells)
{ CHECK(game.GetBoard().Set(MakePattern(game.GetBoard().GetSize(), cells))); }

/// Straightforward implementation of B3/S23 where cells outside the board are dead.
class ReferenceGame
{
    Size              size;
    std::vector<bool> cells;

public:
    ReferenceGame(const Size& size, const Cells& alive) : size(size), cells(size_t(size.cx * size.cy), false)
    {
        for (const auto& [x, y] : alive)
            cells[Index(x, y)] = true;
    }

    void Next()
    {
        std::vector<bool> next(cells.size(), false);
        for (Integer y = 0; y < size.cy; y++) {
            for (Integer x = 0; x < size.cx; x++) {
                auto count = 0;
                for (auto dy = -1; dy <= 1; dy++) {
                    for (auto dx = -1; dx <= 1; dx++) {
                        if ((dx != 0 || dy != 0) && Get(x + dx, y + dy))
                            count++;
                    }
                }
                next[Index(x, y)] = count == 3 || (count == 2 && Get(x, y));
            }
        }
        cells = next;
    }

    Cells GetAliveCells() const
    {
        Cells result;
        for (Integer y = 0; y < size.cy; y++) {
            for (Integer x = 0; x < size.cx; x++) {
                if (Get(x, y))
                    result.insert({ x, y });
            }
        }
        return result;
    }

private:
    size_t Index(Integer x, Integer y) const
    { return size_t(y * size.cx + x); }

    bool Get(Integer x, Integer y) const
    { return 0 <= x && x < size.cx && 0 <= y && y < size.cy && cells[Index(x, y)]; }
};

Cells MakeRandomCells(const Size& size, unsigned int seed, int percentage)
{
    std::mt19937                       engine(seed);
    std::uniform_int_distribution<int> distribution(0, 99);
    Cells                              cells;
    for (Integer y = 0; y < size.cy; y++) {
        for (Integer x = 0; x < size.cx; x++) {
            if (distribution(engine) < percentage)
                cells.insert({ x, y });
        }
    }
    return cells;
}

/// A temporary folder for pattern files. It is removed at the end of the scope.
class TemporaryFolder
{
    std::filesystem::path path;

public:
    TemporaryFolder()
    {
        static auto counter = 0;
        std::random_device random;
        path = std::filesystem::temp_directory_path() /
               ("ShosLifeGameTest_" + std::to_string(random()) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TemporaryFolder()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    TemporaryFolder(const TemporaryFolder&)            = delete;
    TemporaryFolder& operator=(const TemporaryFolder&) = delete;

    const std::filesystem::path& GetPath() const
    { return path; }

    void Write(const std::string& fileName, const std::string& content) const
    {
        std::ofstream stream(path / fileName, std::ios::binary);
        stream << content;
    }
};

std::vector<std::string> ToRows(const Pattern& pattern)
{
    std::vector<std::string> rows;
    const auto size = pattern.GetSize();
    for (Integer y = 0; y < size.cy; y++) {
        std::string row;
        for (Integer x = 0; x < size.cx; x++)
            row += pattern[size_t(y * size.cx + x)] ? '*' : '.';
        rows.push_back(row);
    }
    return rows;
}

/// Reads a single pattern file through PatternSet.
std::vector<std::string> ReadRows(const std::string& fileName, const std::string& content)
{
    TemporaryFolder folder;
    folder.Write(fileName, content);
    PatternSet patternSet(ToTString(folder.GetPath()));
    CHECK_EQ(size_t(1), patternSet.GetSize());
    return ToRows(patternSet[0]);
}

std::filesystem::path cellDataFolder;

const Pattern* FindPattern(const PatternSet& patternSet, const std::string& name)
{
    for (size_t index = 0; index < patternSet.GetSize(); index++) {
        if (patternSet[index].GetName() == ToTString(name))
            return &patternSet[index];
    }
    return nullptr;
}

/// Reads "x = .., y = .." in an RLE file.
Size ReadRleHeaderSize(const std::filesystem::path& filePath)
{
    std::ifstream stream(filePath);
    std::string   line;
    while (std::getline(stream, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), [](unsigned char character) { return std::isspace(character); }), line.end());
        if (!line.starts_with("x="))
            continue;
        Size size;
        for (const auto& token : String::Split(line, ',')) {
            if (token.starts_with("x="))
                size.cx = std::stoi(token.substr(2));
            else if (token.starts_with("y="))
                size.cy = std::stoi(token.substr(2));
        }
        return size;
    }
    return Size();
}

std::string ToText(const Size& size)
{ return "(" + std::to_string(size.cx) + " x " + std::to_string(size.cy) + ")"; }

// ---- Simulation tests (must pass in every combination of options) ----------

const Cells glider = { { 1, 0 }, { 2, 1 }, { 0, 2 }, { 1, 2 }, { 2, 2 } };

void BlockIsStillLife()
{
    Game game(Size(16, 16));
    const Cells block = { { 7, 7 }, { 8, 7 }, { 7, 8 }, { 8, 8 } };
    Place(game, block);
    for (auto generation = 0; generation < 10; generation++) {
        game.Next();
        CHECK_EQ(block, GetAliveCells(game.GetBoard()));
    }
}

void BlinkerHasPeriod2()
{
    Game game(Size(16, 16));
    const Cells horizontal = { { 6, 8 }, { 7, 8 }, { 8, 8 } };
    const Cells vertical   = { { 7, 7 }, { 7, 8 }, { 7, 9 } };
    Place(game, horizontal);
    for (auto generation = 1; generation <= 10; generation++) {
        game.Next();
        CHECK_EQ(generation % 2 == 0 ? horizontal : vertical, GetAliveCells(game.GetBoard()));
    }
}

void GliderMovesDiagonally()
{
    Game game(Size(32, 32));
    const auto start = Shift(glider, 4, 4);
    Place(game, start);
    for (auto period = 1; period <= 5; period++) {
        for (auto generation = 0; generation < 4; generation++)
            game.Next();
        CHECK_EQ(Shift(start, period, period), GetAliveCells(game.GetBoard()));
    }
}

void GenerationIsCounted()
{
    Game game(Size(16, 16));
    CHECK_EQ(0ULL, game.GetGeneration());
    for (auto generation = 0; generation < 3; generation++)
        game.Next();
    CHECK_EQ(3ULL, game.GetGeneration());
}

void OutsideOfBoardIsDead()
{
    // A blinker on the top edge. On a torus, a cell at the bottom row would be born.
    Game game(Size(16, 16));
    Place(game, { { 6, 0 }, { 7, 0 }, { 8, 0 } });
    game.Next();
    CHECK_EQ((Cells{ { 7, 0 }, { 7, 1 } }), GetAliveCells(game.GetBoard()));
    game.Next();
    CHECK_EQ(Cells{}, GetAliveCells(game.GetBoard()));
}

void GliderVanishesAtCorner()
{
    // A glider running into the bottom-right corner becomes a block (not wrapped around).
    const Size size(16, 16);
    Game       game(size);
    const auto start = Shift(glider, 9, 9);
    Place(game, start);

    ReferenceGame reference(size, start);
    for (auto generation = 0; generation < 40; generation++) {
        game.Next();
        reference.Next();
        CHECK_EQ(reference.GetAliveCells(), GetAliveCells(game.GetBoard()));
    }
    for (const auto& [x, y] : GetAliveCells(game.GetBoard()))
        CHECK(x >= 9 && y >= 9);
}

void PatternIsCentered()
{
    Board board(Size(16, 16));
    CHECK(board.Set(Pattern(_T("glider"), ".*...****", 3U)));
    CHECK_EQ(Shift(glider, 6, 6), GetAliveCells(board));
}

void AreaSurroundsPattern()
{
#if defined(AREA)
    // Review 3.7: the area of a cleared board must start at the center, not at the left-top corner.
    Board board(Size(32, 32));
    CHECK(board.Set(Pattern(_T("glider"), ".*...****", 3U)));
    const auto area = board.GetArea();
    CHECK(!area.IsIn(Point(0, 0)));
    for (const auto& [x, y] : GetAliveCells(board))
        CHECK(area.IsIn(Point(x, y)));
#endif // AREA
}

void AreaShrinks()
{
#if defined(AREA)
    // Review 3.9: a lone cell dies, so that the area shrinks to the block.
    Game game(Size(64, 64));
    Place(game, { { 5, 5 }, { 6, 5 }, { 5, 6 }, { 6, 6 }, { 50, 50 } });
    game.Next();
    game.Next();
    const auto area = game.GetBoard().GetArea();
    CHECK(!area.IsIn(Point(50, 50)));
    CHECK(area.size.cx <= 4 && area.size.cy <= 4);
#endif // AREA
}

void AreaFollowsGlider()
{
#if defined(AREA)
    Game       game(Size(64, 64));
    const auto start = Shift(glider, 2, 2);
    Place(game, start);
    for (auto generation = 0; generation < 120; generation++)
        game.Next();
    CHECK_EQ(Shift(start, 30, 30), GetAliveCells(game.GetBoard()));
    const auto area = game.GetBoard().GetArea();
    CHECK(!area.IsIn(Point(2, 2)));
    CHECK(area.size.cx <= 5 && area.size.cy <= 5);
#endif // AREA
}

void AllCellsDie()
{
    Game game(Size(16, 16));
    Place(game, { { 3, 3 }, { 10, 10 } });
    game.Next();
    CHECK_EQ(Cells{}, GetAliveCells(game.GetBoard()));
    Place(game, { { 7, 6 }, { 7, 7 }, { 7, 8 } });
    game.Next();
    CHECK_EQ((Cells{ { 6, 7 }, { 7, 7 }, { 8, 7 } }), GetAliveCells(game.GetBoard()));
}

void TooLargePatternIsRejected()
{
    Board board(Size(8, 8));
    CHECK(!board.Set(Pattern(_T("wide"), std::string(9, '*'), 9U)));
}

void RandomBoardMatchesReference()
{
    const struct { Size size; unsigned int seed; int percentage; } cases[] = {
        { Size(64, 40),  1U, 35 },
        { Size(40, 64),  2U, 50 },
        { Size(24,  8),  3U, 30 },
        { Size(72, 72),  4U, 10 },
    };
    for (const auto& testCase : cases) {
        const auto cells = MakeRandomCells(testCase.size, testCase.seed, testCase.percentage);
        Game       game(testCase.size);
        Place(game, cells);
        ReferenceGame reference(testCase.size, cells);

        for (auto generation = 0; generation < 80; generation++) {
            game.Next();
            reference.Next();
            CHECK_EQ(reference.GetAliveCells(), GetAliveCells(game.GetBoard()));
        }
    }
}

void ResetRestartsGeneration()
{
    Game game(Size(16, 16));
    game.Next();
    game.Reset(false);
    CHECK_EQ(0ULL, game.GetGeneration());
}

// ---- Bitmap layout tests (review 3.5, 3.6) ---------------------------------
//
// The bits are passed to a monochrome DDB (ShosLifeGameBoardPainter.h).
// A DDB row is WORD-aligned and its most significant bit is the leftmost pixel.

template <typename TAction>
void WithBits(const Size& size, const Cells& cells, TAction action)
{
    auto game = std::make_unique<Game>(size);
    Place(*game, cells);
    action(game->GetBoard().GetBits());
}

void BitmapLeftmostPixelIsMostSignificantBit()
{
    WithBits(Size(48, 4), { { 0, 0 }, { 9, 0 } }, [](const UnitInteger* bits) {
        CHECK_EQ(0x80U, unsigned(bits[0]));
        CHECK_EQ(0x40U, unsigned(bits[1]));
    });
}

void BitmapRowIsWordAlignedPackedBits()
{
    const Integer width  = 40;
    const auto    stride = size_t((width + 15) / 16 * 2); // bytes per row of the DDB
    CHECK_EQ(stride, size_t(BitCellSet::GetRowByteNumber(width)));
    WithBits(Size(width, 4), { { 0, 1 } }, [&](const UnitInteger* bits) {
        CHECK(bits[stride] != 0);
    });
}

void BitmapMatchesBoard()
{
    // Every pixel of the bitmap (including widths that are not a multiple of 8) matches the board.
    for (const auto& size : { Size(64, 40), Size(37, 11), Size(17, 5) }) {
        const auto cells = MakeRandomCells(size, 5U, 40);
        WithBits(size, cells, [&](const UnitInteger* bits) {
            const auto stride = size_t(BitCellSet::GetRowByteNumber(size.cx));
            Cells      pixels;
            for (Integer y = 0; y < size.cy; y++) {
                for (Integer x = 0; x < size.cx; x++) {
                    if ((bits[size_t(y) * stride + size_t(x / 8)] & (0x80 >> (x % 8))) != 0)
                        pixels.insert({ x, y });
                }
            }
            CHECK_EQ(cells, pixels);
        });
    }
}

// ---- Pattern file tests (review 3.3, 3.4, 3.12) ----------------------------

void LifBasic()
{
    const auto rows = ReadRows("glider.lif", "#Life 1.05\n#D Glider\n#N\n#P -1 -1\n.*\n..*\n***\n");
    CHECK_EQ((std::vector<std::string>{ ".*.", "..*", "***" }), rows);
}

void LifNameIsFileStem()
{
    TemporaryFolder folder;
    folder.Write("MyGlider.lif", "#Life 1.05\n.*\n..*\n***\n");
    PatternSet patternSet(ToTString(folder.GetPath()));
    CHECK_EQ(size_t(1), patternSet.GetSize());
    CHECK(patternSet[0].GetName() == ToTString("MyGlider"));
}

void LifSkipsBangCommentLines()
{
    const auto rows = ReadRows("comment.lif", "#Life 1.05\n!Name: a long comment line\n.*\n**\n");
    CHECK_EQ((std::vector<std::string>{ ".*", "**" }), rows);
}

void LifSkipsLeadingAndTrailingBlankLines()
{
    const auto rows = ReadRows("blank.lif", "#Life 1.05\n\n.*\n**\n\n");
    CHECK_EQ((std::vector<std::string>{ ".*", "**" }), rows);
}

void LifInnerBlankLineIsDeadRow()
{
    // e.g. 13_engine_Cordership.lif
    const auto rows = ReadRows("inner.lif", "#Life 1.05\n.*\n\n**\n");
    CHECK_EQ((std::vector<std::string>{ ".*", "..", "**" }), rows);
}

void LifUpperCaseExtension()
{
    TemporaryFolder folder;
    folder.Write("GLIDER.LIF", "#Life 1.05\n.*\n..*\n***\n");
    folder.Write("glider2.life", "#Life 1.05\n.*\n..*\n***\n");
    PatternSet patternSet(ToTString(folder.GetPath()));
    CHECK_EQ(size_t(2), patternSet.GetSize());
}

void LifMultipleBlocks()
{
    // "#P x y" places the following lines at (x, y).
    const auto rows = ReadRows("blocks.lif", "#Life 1.05\n#P -2 -1\n**\n#P 1 1\n*\n.*\n");
    CHECK_EQ((std::vector<std::string>{ "**...", ".....", "...*.", "....*" }), rows);
}

void LifCrLf()
{
    const auto rows = ReadRows("crlf.lif", "#Life 1.05\r\n.*\r\n**\r\n");
    CHECK_EQ((std::vector<std::string>{ ".*", "**" }), rows);
}

void PatternsAreSortedByFileName()
{
    TemporaryFolder folder;
    for (const auto name : { "b.lif", "C.rle", "A.LIF", "a.rle", "c.lif" })
        folder.Write(name, std::string(name).ends_with("rle") ? "x = 1, y = 1\no!\n" : "*\n");
    PatternSet patternSet(ToTString(folder.GetPath()));
    std::vector<tstring> names;
    for (size_t index = 0; index < patternSet.GetSize(); index++)
        names.push_back(patternSet[index].GetName());
    // Life 1.05 files first, then RLE files. Each group is sorted case-insensitively.
    CHECK((names == std::vector<tstring>{ ToTString("A"), ToTString("b"), ToTString("c"), ToTString("a"), ToTString("C") }));
}

void RleBasic()
{
    const auto rows = ReadRows("glider.rle", "#N Glider\nx = 3, y = 3, rule = B3/S23\nbo$2bo$3o!\n");
    CHECK_EQ((std::vector<std::string>{ ".*.", "..*", "***" }), rows);
}

void RleMultiDigitCountAndMultipleLines()
{
    // "$$" ends a line and then an empty line.
    const auto rows = ReadRows("long.rle", "x = 12, y = 3\n12o$$\n3b2o!\n");
    CHECK_EQ((std::vector<std::string>{ "************", "............", "...**......." }), rows);
}

void RleUpperCaseTags()
{
    const auto rows = ReadRows("upper.rle", "x = 3, y = 2\n3O$B2O!\n");
    CHECK_EQ((std::vector<std::string>{ "***", ".**" }), rows);
}

void RleRunOfLineBreaks()
{
    const auto rows = ReadRows("gap.rle", "x = 2, y = 4\n2o3$2o!\n");
    CHECK_EQ((std::vector<std::string>{ "**", "..", "..", "**" }), rows);
}

void RleCrLf()
{
    const auto rows = ReadRows("crlf.rle", "#N name\r\n \r\nx = 3, y = 2\r\n3o$\r\nbo!\r\n");
    CHECK_EQ((std::vector<std::string>{ "***", ".*." }), rows);
}

void RleOmittedTrailingEmptyLines()
{
    const auto rows = ReadRows("trailing.rle", "x = 2, y = 3\n2o!\n");
    CHECK_EQ((std::vector<std::string>{ "**", "..", ".." }), rows);
}

void RleWithoutTerminator()
{
    const auto rows = ReadRows("open.rle", "x = 2, y = 2\n2o$bo\n");
    CHECK_EQ((std::vector<std::string>{ "**", ".*" }), rows);
}

// ---- Bundled CellData tests ------------------------------------------------

void BundledPatternsAreAllLoaded()
{
    size_t fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(cellDataFolder)) {
        const auto extension = String::ToLower(entry.path().extension().string());
        if (extension == ".lif" || extension == ".life" || extension == ".rle")
            fileCount++;
    }
    CHECK(fileCount > 0);
    PatternSet patternSet(ToTString(cellDataFolder));
    CHECK_EQ(fileCount, patternSet.GetSize());
}

void CheckBundledRleSize(const std::string& name)
{
    PatternSet     patternSet(ToTString(cellDataFolder));
    const Pattern* pattern = FindPattern(patternSet, name);
    CHECK(pattern != nullptr);
    CHECK_EQ(ReadRleHeaderSize(cellDataFolder / (name + ".rle")), pattern->GetSize());
}

void BundledRleSizesMatchHeader()
{
    for (const auto& entry : std::filesystem::directory_iterator(cellDataFolder)) {
        const auto name = entry.path().stem().string();
        if (entry.path().extension() == ".rle" && name != "OTCA_metapixel")
            CheckBundledRleSize(name);
    }
}

void BundledOtcaMetapixelSizeMatchesHeader()
{ CheckBundledRleSize("OTCA_metapixel"); }

void BundledCordershipStartsWithCells()
{
    PatternSet     patternSet(ToTString(cellDataFolder));
    const Pattern* pattern = FindPattern(patternSet, "13_engine_Cordership");
    CHECK(pattern != nullptr);
    const auto rows = ToRows(*pattern);
    CHECK(!rows.empty());
    CHECK(rows.front().find('*') != std::string::npos);
}

// ---- Runner ----------------------------------------------------------------

std::string GetConfiguration()
{
    std::string configuration;
#if defined(USEBITS)
    configuration += "USEBITS ";
#endif // USEBITS
#if defined(FAST)
    configuration += "FAST ";
#endif // FAST
#if defined(MT)
    configuration += "MT ";
#endif // MT
#if defined(AREA)
    configuration += "AREA ";
#endif // AREA
    return configuration.empty() ? "(none)" : configuration;
}

int Run()
{
    const std::vector<TestCase> testCases = {
        { "BlockIsStillLife"                         , Expect::Pass    , ""   , BlockIsStillLife                        },
        { "BlinkerHasPeriod2"                        , Expect::Pass    , ""   , BlinkerHasPeriod2                       },
        { "GliderMovesDiagonally"                    , Expect::Pass    , ""   , GliderMovesDiagonally                   },
        { "GenerationIsCounted"                      , Expect::Pass    , ""   , GenerationIsCounted                     },
        { "ResetRestartsGeneration"                  , Expect::Pass    , ""   , ResetRestartsGeneration                 },
        { "OutsideOfBoardIsDead"                     , Expect::Pass    , ""   , OutsideOfBoardIsDead                    },
        { "GliderVanishesAtCorner"                   , Expect::Pass    , ""   , GliderVanishesAtCorner                  },
        { "PatternIsCentered"                        , Expect::Pass    , ""   , PatternIsCentered                       },
        { "AreaSurroundsPattern"                     , Expect::Pass    , ""   , AreaSurroundsPattern                    },
        { "AreaShrinks"                              , Expect::Pass    , ""   , AreaShrinks                             },
        { "AreaFollowsGlider"                        , Expect::Pass    , ""   , AreaFollowsGlider                       },
        { "AllCellsDie"                              , Expect::Pass    , ""   , AllCellsDie                             },
        { "TooLargePatternIsRejected"                , Expect::Pass    , ""   , TooLargePatternIsRejected               },
        { "RandomBoardMatchesReference"              , Expect::Pass    , ""   , RandomBoardMatchesReference             },
        { "BitmapLeftmostPixelIsMostSignificantBit"  , Expect::Pass    , ""   , BitmapLeftmostPixelIsMostSignificantBit },
        { "BitmapRowIsWordAlignedPackedBits"         , Expect::Pass    , ""   , BitmapRowIsWordAlignedPackedBits        },
        { "BitmapMatchesBoard"                       , Expect::Pass    , ""   , BitmapMatchesBoard                      },
        { "LifBasic"                                 , Expect::Pass    , ""   , LifBasic                                },
        { "LifNameIsFileStem"                        , Expect::Pass    , ""   , LifNameIsFileStem                       },
        { "LifSkipsBangCommentLines"                 , Expect::Pass    , ""   , LifSkipsBangCommentLines                },
        { "LifSkipsLeadingAndTrailingBlankLines"     , Expect::Pass    , ""   , LifSkipsLeadingAndTrailingBlankLines    },
        { "LifInnerBlankLineIsDeadRow"               , Expect::Pass    , ""   , LifInnerBlankLineIsDeadRow              },
        { "LifUpperCaseExtension"                    , Expect::Pass    , ""   , LifUpperCaseExtension                   },
        { "LifMultipleBlocks"                        , Expect::Pass    , ""   , LifMultipleBlocks                       },
        { "LifCrLf"                                  , Expect::Pass    , ""   , LifCrLf                                 },
        { "PatternsAreSortedByFileName"              , Expect::Pass    , ""   , PatternsAreSortedByFileName             },
        { "RleBasic"                                 , Expect::Pass    , ""   , RleBasic                                },
        { "RleMultiDigitCountAndMultipleLines"       , Expect::Pass    , ""   , RleMultiDigitCountAndMultipleLines      },
        { "RleUpperCaseTags"                         , Expect::Pass    , ""   , RleUpperCaseTags                        },
        { "RleRunOfLineBreaks"                       , Expect::Pass    , ""   , RleRunOfLineBreaks                      },
        { "RleCrLf"                                  , Expect::Pass    , ""   , RleCrLf                                 },
        { "RleWithoutTerminator"                     , Expect::Pass    , ""   , RleWithoutTerminator                    },
        { "RleOmittedTrailingEmptyLines"             , Expect::Pass    , ""   , RleOmittedTrailingEmptyLines            },
        { "BundledPatternsAreAllLoaded"              , Expect::Pass    , ""   , BundledPatternsAreAllLoaded             },
        { "BundledRleSizesMatchHeader"               , Expect::Pass    , ""   , BundledRleSizesMatchHeader              },
        { "BundledOtcaMetapixelSizeMatchesHeader"    , Expect::Pass    , ""   , BundledOtcaMetapixelSizeMatchesHeader   },
        { "BundledCordershipStartsWithCells"         , Expect::Pass    , ""   , BundledCordershipStartsWithCells        },
    };

    std::cout << std::unitbuf; // Keep the output even if a test crashes.
    std::cout << "Configuration: " << GetConfiguration() << "\n";

    auto passed = 0, failed = 0, expectedFailures = 0, unexpectedPasses = 0;
    for (const auto& testCase : testCases) {
        std::string message;
        try {
            testCase.run();
        } catch (const TestFailure& failure) {
            message = failure.message;
        } catch (const std::exception& exception) {
            message = std::string("exception: ") + exception.what();
        }

        const auto ok = message.empty();
        if (testCase.expect == Expect::Pass) {
            if (ok) {
                passed++;
                std::cout << "  PASS  " << testCase.name << "\n";
            } else {
                failed++;
                std::cout << "  FAIL  " << testCase.name << "\n      " << message << "\n";
            }
        } else {
            if (ok) {
                unexpectedPasses++;
                std::cout << "  XPASS " << testCase.name << " (review " << testCase.note << " seems fixed: change it to Expect::Pass)\n";
            } else {
                expectedFailures++;
                std::cout << "  XFAIL " << testCase.name << " (review " << testCase.note << ")\n";
            }
        }
    }

    std::cout << "Result: " << passed << " passed, " << failed << " failed, "
              << expectedFailures << " known bugs, " << unexpectedPasses << " unexpectedly passed" << std::endl;
    return failed == 0 && unexpectedPasses == 0 ? 0 : 1;
}

} // namespace Shos::LifeGame::Test

int main(int argc, char* argv[])
{
    using namespace Shos::LifeGame::Test;
    cellDataFolder = argc > 1 ? std::filesystem::path(argv[1])
                              : std::filesystem::path(__FILE__).parent_path().parent_path() / "Shos.LifeGame" / "CellData";
    return Run();
}
