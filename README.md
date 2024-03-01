# Shos.LifeGame

&quot;C++/Win32 ultra-fast life game&quot;

[Conway's Game of Life | Wikipedia](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)

![LifeGame](https://github.com/Fujiwo/Shos.LifeGame/blob/ad75f0d9918889e01b941cb3e861f661ea3fed34/Images/lifegame01.png)

![LifeGame](https://github.com/Fujiwo/Shos.LifeGame/blob/d3f10a69e5d811110e2ce61c762cc321bfd114af/Images/lifefame01.20240112.161130.gif)

<div><video controls src="https://github.com/Fujiwo/Shos.LifeGame/blob/6dac05c4254f9cd7b37e7fe0f8287ea8ea9d6233/Images/lifefame01.20240112.161130.mp4" muted="false"></video></div>

[Video](https://capture.dropbox.com/8Pu3gPGy8BBhdK6W)

This program is designed to simulate a cellular automaton known as the &quot;Life Game&quot;. Below are the main classes and their roles:

- Random: A class for generating random numbers. It uses std::random_device and std::mt19937.
- Size, Point, Rect: These are classes representing size, coordinates, and rectangles, respectively. They are used to manage the game field and the position of cells.
- Utility: This class provides methods to perform actions on each point within a given rectangle. It is used to scan all cells.
- ThreadUtility: A class to support multithreaded processing. It performs actions in parallel for a specific range of integers.
- Pattern, PatternSet: Classes to represent the initial patterns of the &quot;Life Game&quot;. Patterns are stored as strings representing whether a cell is alive or dead.
- BitCellSet: A class to represent the game field. Each cell is represented as a bit.

This program includes several optimizations to improve performance, such as multithreading and fast loops. These can be enabled or disabled through preprocessor directives (#define). For example, #define FAST enables fast loops, and #define MT enables multithreaded processing. Also, #define AREA enables optimization to track the area of active cells and reduce unnecessary calculations. These directives can be used to adjust the performance and resource usage of the program.

Overall, this program provides various features and optimizations to efficiently simulate the &quot;Life Game&quot;. Each class and function is designed to serve a specific purpose. By understanding this program, you can gain a deep understanding of many important computer science concepts, such as game simulation, multithreaded processing, and performance optimization.
