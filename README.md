# Shos.LifeGame

![LifeGame](Images/lifegame01.20240112.161130.gif)

[![LifeGame](https://github.com/Fujiwo/Shos.LifeGame/blob/ad75f0d9918889e01b941cb3e861f661ea3fed34/Images/lifegame01.png)](https://youtu.be/ny-VKZEAVkg)

## Overview

"C++/Win32 ultra-fast life game"

[Conway's Game of Life | Wikipedia](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)

This program is designed to simulate a cellular automaton known as the "Life Game".

## Program

Below are the main classes and their roles:

### namespace Shos::LifeGame

- Random: A class for generating random numbers. It uses std::random_device and std::mt19937.
- Size, Point, Rect: These are classes representing size, coordinates, and rectangles, respectively. They are used to manage the game field and the position of cells.
- Utility: This class provides methods to perform actions on each point within a given rectangle. It is used to scan all cells.
- ThreadUtility: A class to support multithreaded processing. It performs actions in parallel for a specific range of integers.
- Pattern, PatternSet: Classes to represent the initial patterns of the "Life Game". Patterns are stored as strings representing whether a cell is alive or dead.
- BitCellSet: A class to represent the game field. Each cell is represented as a bit.
- Game: The main class of the program. It manages the game field and the rules of the "Life Game". It also provides methods to perform the game simulation.

This namespace includes several optimizations to improve performance, such as data representation switching, multithreading, and fast loops. These can be enabled or disabled through preprocessor directives (#define). `#define USEBITS` enables 1-bit-per-cell storage (when it is not defined, the board uses an array of `bool` rows). `#define FAST` enables fast loops, `#define MT` enables multithreaded processing, and `#define AREA` enables optimization to track the area of active cells and reduce unnecessary calculations. In this project, these four directives are treated as the four core optimization elements, while `BoardPainter` is treated separately as rendering optimization. These directives can be used to adjust the performance and resource usage of the program.

Overall, this namespace provides various features and optimizations to efficiently simulate the "Life Game". Each class and function is designed to serve a specific purpose. By understanding this program, you can gain a deep understanding of many important computer science concepts, such as game simulation, multithreaded processing, and performance optimization.

### namespace Shos::LifeGame::Win32

- BoardPainter: A class to be responsible for painting a game board on a Windows device context, and for rendering optimization.

### namespace Shos::LifeGame::Application

- MainWindow: A class to manage the main window of the program. It handles user input and displays the game field.
- Program: The main class of the program. It creates the main window and starts the game simulation.

### namespace Shos::Win32

- Window: A class to manage a Windows window. It provides methods to create and manage a window.
- Program: A class to manage the main loop of a Windows application. It provides methods to start and stop the main loop.

This namespace provides a basic framework for creating a window and running a message loop using the Windows API.

### namespace Shos

- Helper: A class to provide generic helper methods for collections (ForEach, Map, Filter, Maximum and Connect).
- File: A class to manage files. It provides methods to list the files in a folder and to read a text file.
- String: A class to manage strings. It provides methods to trim, pad, split and lower strings.
- stopwatch: A class to measure time. It uses std::chrono::steady_clock.

## Usage

- The board has 1000 x 1000 cells. Cells outside of the board are always dead (the board is not a torus).
- Press '0' to '9', 'a' to 'z' or 'A' to 'Z' to choose a pattern. If you press a key that is not assigned a pattern, a random pattern will be generated.
- Click the right mouse button to generate a random pattern.
- The title bar shows the generation, the elapsed time, the FPS (generations per second) and the pattern name.

## Pattern Files

Pattern files are read from the CellData folder next to the executable.

The program supports the following two pattern file formats (the extensions are case-insensitive):

- [Life 1.05](https://conwaylife.com/wiki/Life_1.05) (.lif/.life): '#' lines and '!' comment lines are skipped. '#P x y' blocks are supported.
- [RLE](https://conwaylife.com/wiki/Run_Length_Encoded) (.rle): The rule in the header is ignored (B3/S23 is always used).

By default, the CellData folder contains 46 pattern files. Life 1.05 files come first and RLE files follow, each sorted by file name. They are assigned to '0' to '9', 'a' to 'z' and 'A' to 'J' in this order.

## Tests

Shos.LifeGame.Test/Shos.LifeGame.RegressionTest.cpp contains regression tests of the simulation, the bitmap layout and the pattern files. The script below builds and runs them for all 16 combinations of USEBITS / FAST / MT / AREA with g++ (Linux or WSL).

```
./Shos.LifeGame.Test/run_regression_tests.sh          # all combinations
./Shos.LifeGame.Test/run_regression_tests.sh --asan   # with AddressSanitizer and UndefinedBehaviorSanitizer
```

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

