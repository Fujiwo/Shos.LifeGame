# Shos.LifeGame

[English](README.md) | [日本語](README.jp.md)

![LifeGame](Images/lifegame01.20240112.161130.gif)

[![LifeGame](Images/lifegame01.png)](https://youtu.be/ny-VKZEAVkg)

**An ultra-fast [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life) (B3/S23) written only in C++20 and the Win32 API.**

This repository is also teaching material on optimization. Each optimization can be turned on and off with a `#define`, so you can measure how much each one speeds up the program. A Japanese article explains the code step by step ([Articles(Japanese)/ShosLifeGame_Article.md](Articles(Japanese)/ShosLifeGame_Article.md)).

## Highlights

- **About 15 times faster with four optimizations**: 16 → 244 generations per second on a 1000 x 1000 board (see [Benchmark](#benchmark)).
- **Each optimization is a `#define`**: comment one out, rebuild, and compare the FPS in the title bar.
- **The board is drawn without converting it**: the bits of the board are passed as they are to a 1bpp bitmap and drawn with `BitBlt`.
- **No external libraries**: only the C++20 standard library and the Win32 API.
- **All 16 combinations are tested**: the regression tests build and run every combination of the four `#define`s on Linux/WSL, also on GitHub Actions.
- **46 well-known patterns are included**: guns, spaceships, puffers, methuselahs, and more, in Life 1.05 and RLE formats.

## Optimizations

The four `#define`s at the top of [Shos.LifeGame/ShosLifeGame.h](Shos.LifeGame/ShosLifeGame.h) are the core optimizations. All of them are defined by default.

```cpp
#define USEBITS // Bits enabled           : a cell is stored in one bit (otherwise in one bool).
#define FAST    // Fast loops enabled     : direct loops instead of generic loops with std::function.
#define MT      // Multi-threading enabled: the rows are split among threads.
#define AREA    // Area enabled           : only the active area of the board is computed.
```

| `#define` | What it does | Trade-off |
|---|---|---|
| `USEBITS` | Stores one cell in one bit (`BitCellSet`). When undefined, each row is an array of `bool`. | Bit operations are harder to read than `bool` arrays. |
| `FAST` | Uses direct loops instead of a generic loop with `std::function` (`Utility::ForEach`). The next generation is computed row by row (`Game::NextRow`): the sum of the 3 cells in a column is shared by the neighboring cells. | Some code is duplicated. |
| `MT` | Splits the rows (Y direction) among the threads of `ThreadPool`, and each thread runs `NextPart`. The threads are kept for the whole game, and a small area is computed by fewer threads. | Less effective on small boards. |
| `AREA` | Computes only the active area: the bounding box of the live cells plus a margin of one cell. | Managing the area is complex. Less effective when the cells spread over the board. |

Besides these four, `BoardPainter` ([Shos.LifeGame/ShosLifeGameBoardPainter.h](Shos.LifeGame/ShosLifeGameBoardPainter.h)) optimizes drawing. When `USEBITS` is defined, `BoardPainter` writes the bits of the board directly to a bitmap with `SetBitmapBits`, and draws it with `BitBlt`. The bitmap is created once and reused. To make this possible, the bit layout of `BitCellSet` follows the 1bpp bitmap format: the most significant bit of each byte is the leftmost pixel, and each row is aligned to a WORD boundary.

## Benchmark

Each level adds one optimization to the previous level.

| Level | `USEBITS` | `FAST` | `MT` | `AREA` | Time | Generations/s | Speed-up |
|---:|:---:|:---:|:---:|:---:|---:|---:|---:|
| 0 | - | - | - | - | 63.8 s | 16 | x1.00 |
| 1 | ✓ | - | - | - | 60.2 s | 17 | x1.06 |
| 2 | ✓ | ✓ | - | - | 24.2 s | 41 | x2.56 |
| 3 | ✓ | ✓ | ✓ | - | 11.7 s | 86 | x5.38 |
| 4 | ✓ | ✓ | ✓ | ✓ | 4.1 s | 244 | x15.25 |

- Conditions: pattern `Gun_Gunstar`, 1000 x 1000 board, 1000 generations, Release build.
- Environment: DELL XPS 13 9310, 11th Gen Intel Core i7 2.92 GHz (4 cores, 8 logical processors), 16 GB memory, Windows 11.
- The results depend on the CPU, the build settings and the pattern. For example, `AREA` is less effective with a random board, because the active area covers the whole board.

The raw data is in [Articles(Japanese)/ShosLifeGame_BenchmarkData.csv](Articles(Japanese)/ShosLifeGame_BenchmarkData.csv).

## Build and Run

Requirements: Windows, Visual Studio with the C++ desktop workload (platform toolset v145, C++20).

1. Open `Shos.LifeGame.sln` in Visual Studio.
2. Choose `Release` and `x64`, and build. The post-build event copies the `CellData` folder to the output folder.
3. Run `Shos.LifeGame`.

To compare the optimizations, comment out some of the `#define`s in `ShosLifeGame.h`, rebuild, and check the FPS in the title bar. If you uncomment `#define LAP_TIMES 1000` in `Shos.LifeGame.cpp`, the title bar keeps showing the values at generation 1000, which is the condition of the benchmark.

## Usage

| Operation | Action |
|---|---|
| `0`-`9`, `a`-`z`, `A`-`J` | Load a pattern from `CellData` |
| Other keys (`K`-`Z` etc.) | Fill the board at random |
| Right click | Fill the board at random |

- The board has 1000 x 1000 cells. Cells outside the board are always dead (the board is not a torus).
- The title bar shows the generation, the elapsed time, the FPS (generations per second) and the pattern name.
- The program advances one generation whenever the message queue is empty. It keeps one CPU core busy on purpose, to measure the maximum FPS.

## Pattern Files

Pattern files are read from the `CellData` folder next to the executable. Two formats are supported (the extensions are case-insensitive):

- [Life 1.05](https://conwaylife.com/wiki/Life_1.05) (`.lif`/`.life`): `#` lines and `!` comment lines are skipped. `#P x y` blocks are supported.
- [RLE](https://conwaylife.com/wiki/Run_Length_Encoded) (`.rle`): the rule in the header is ignored (B3/S23 is always used).

The files are sorted with Life 1.05 files first and RLE files next, each by file name (case-insensitive). They are assigned to the keys `0`-`9`, `a`-`z` and `A`-`J` in this order. Adding or renaming a file shifts the key assignment.

## Tests

[Shos.LifeGame.Test/Shos.LifeGame.RegressionTest.cpp](Shos.LifeGame.Test/Shos.LifeGame.RegressionTest.cpp) contains regression tests of the simulation, the active area, the bitmap layout and the pattern files. The simulation results are compared with a simple reference implementation. The script below builds and runs the tests for all 16 combinations of `USEBITS`/`FAST`/`MT`/`AREA` with g++ on Linux or WSL. GitHub Actions runs the same script ([.github/workflows/regression-tests.yml](.github/workflows/regression-tests.yml)).

```
./Shos.LifeGame.Test/run_regression_tests.sh          # all combinations (about 2 minutes)
./Shos.LifeGame.Test/run_regression_tests.sh --asan   # with AddressSanitizer and UndefinedBehaviorSanitizer
```

[Shos.LifeGame.Test/Shos.LifeGame.Benchmark.cpp](Shos.LifeGame.Test/Shos.LifeGame.Benchmark.cpp) measures `Game::Next` without a window. The script below builds it with `-O2` and runs it for the combinations one by one. Each case is run three times, and the best time is shown.

```
./Shos.LifeGame.Test/run_benchmark.sh                        # all combinations
./Shos.LifeGame.Test/run_benchmark.sh --scale 0.1 FAST_MT_AREA USEBITS_FAST_MT_AREA   # fewer generations, some combinations
```

`Shos.LifeGame.Test/Shos.LifeGame.Test.cpp` (the Visual Studio test project) is a benchmark. It does not check the results.

## Repository Structure

```
Shos.LifeGame/                 The application
  ShosLifeGame.h               Game logic, board, patterns and optimizations (platform-independent)
  ShosLifeGameBoardPainter.h   Drawing of the board (Win32 GDI)
  ShosWin32.h                  Small Win32 framework: window and message loop
  ShosHelper.h                 Helpers for collections, files and strings
  ShosStopwatch.h              Stopwatch
  Shos.LifeGame.cpp            Main window and entry point
  CellData/                    Pattern files (.lif/.rle)
Shos.LifeGame.Test/            Regression tests and benchmark (g++), and benchmark (Visual Studio)
Articles(Japanese)/            Japanese article, figures and benchmark data
Images/                        Screenshots and animation
```

## Program

### namespace Shos::LifeGame (`ShosLifeGame.h`)

- `Random`: generates random numbers with `std::random_device` and `std::mt19937`.
- `Size`, `Point`, `Rect`: size, coordinates and rectangles on the board.
- `Utility`: generic loop over the points in a rectangle (used when `FAST` is undefined).
- `ThreadPool`: threads kept for the whole game, which split a range of integers among them (used when `MT` is defined).
- `Pattern`, `PatternSet`: initial patterns, and reading them from Life 1.05 and RLE files.
- `BitCellSet`: a set of cells with one bit per cell, laid out as a 1bpp bitmap.
- `Board`: the board. It derives from `BitCellSet` when `USEBITS` is defined, and uses rows of `bool` otherwise.
- `Game`: computes the next generation from the current board, and manages the active area.

### namespace Shos::LifeGame::Win32 (`ShosLifeGameBoardPainter.h`)

- `BoardPainter`: draws the board on a device context, and optimizes drawing.

### namespace Shos::LifeGame::Application (`Shos.LifeGame.cpp`)

- `MainWindow`: the main window. It handles the keys and the mouse, and shows the board and the FPS.
- `Program`: creates the main window and runs the message loop.

### namespace Shos::Win32 (`ShosWin32.h`)

- `Window`: base class of a window. The messages are dispatched to virtual `On...` handlers.
- `Program`: base class of an application. It runs the message loop.

### namespace Shos (`ShosHelper.h`, `ShosStopwatch.h`)

- `Helper`: generic helpers for collections (`ForEach`, `Map`, `Filter`, `Maximum` and `Connect`).
- `File`: lists the files in a folder and reads a text file.
- `String`: trims, pads, splits and lowers strings.
- `stopwatch`: measures time with `std::chrono::steady_clock`.

## Article (Japanese)

- [C++/Win32で学ぶ高速ライフゲーム実装: Shos.LifeGame徹底解説](Articles(Japanese)/ShosLifeGame_Article.md)

The article explains the rules of the Game of Life, the pattern file formats, the data structures, each optimization and the benchmark, with figures.

## Authors

Fujio Kojima: a software developer in Japan
* Microsoft MVP for Development Tools - Visual C# (Jul. 2005 - Dec. 2014)
* Microsoft MVP for .NET (Jan. 2015 - Oct. 2015)
* Microsoft MVP for Visual Studio and Development Technologies (Nov. 2015 - Jun. 2018)
* Microsoft MVP for Developer Technologies (Nov. 2018 - Jun. 2026)
* [MVP Profile](https://mvp.microsoft.com/en-US/mvp/profile/4185d172-3c9a-e411-93f2-9cb65495d3c4 "MVP Profile")
* [Blog (Japanese)](http://wp.shos.info "Blog (Japanese)")
* [Web Site (Japanese)](http://www.shos.info "Web Site (Japanese)")
* [Twitter](https://twitter.com/Fujiwo)
* [Instagram](https://www.instagram.com/fujiwo/)

## License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.
