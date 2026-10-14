# Shos.LifeGame

[English](README.md) | [日本語](README.jp.md)

![LifeGame](Images/lifegame01.20240112.161130.gif)

[![LifeGame](Images/lifegame01.png)](https://youtu.be/ny-VKZEAVkg)

**An ultra-fast [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life) (B3/S23) written only in C++20 and the Win32 API.**

This repository is also teaching material on optimization. Each optimization can be turned on and off with a `#define`, so you can measure how much each one speeds up the program. A Japanese article explains the code step by step ([Articles(Japanese)/ShosLifeGame_Article.md](Articles(Japanese)/ShosLifeGame_Article.md)).

## Highlights

- **About 15 times faster with four optimizations**: 16 → 244 generations per second on a 1000 x 1000 board (see [Benchmark](#benchmark)). These values were measured on Windows before the speed-up work of October 2026, and must be measured again.
- **Each optimization is a `#define`**: choose a level at the top of `ShosLifeGame.h`, rebuild, and compare the FPS in the title bar.
- **The board is drawn without converting it**: with `USEBITS`, the bits of the board are passed as they are to a 1bpp bitmap and drawn with `BitBlt`.
- **No external libraries**: only the C++20 standard library and the Win32 API.
- **All 16 combinations are tested**: the regression tests build and run every combination of the four `#define`s on Linux/WSL, also on GitHub Actions.
- **46 well-known patterns are included**: guns, spaceships, puffers, methuselahs, and more, in Life 1.05 and RLE formats.

## Optimizations

The four `#define`s at the top of [Shos.LifeGame/ShosLifeGame.h](Shos.LifeGame/ShosLifeGame.h) are the core optimizations. They are chosen by one "level" `#define`. By default, `USEBITS_FAST_MT_AREA` is defined, so all four are defined.

```cpp
//#define USEBITS_ONLY
#define USEBITS_FAST_MT_AREA
#if defined(USEBITS_ONLY)
#define USEBITS // Bits enabled           : a cell is stored in one bit (otherwise in one bool).
#elif defined(USEBITS_FAST)
...
#elif defined(USEBITS_FAST_MT_AREA)
#define USEBITS // Bits enabled           : a cell is stored in one bit (otherwise in one bool).
#define FAST    // Fast loops enabled     : direct loops instead of generic loops with std::function.
#define MT      // Multi-threading enabled: the rows are split among threads.
#define AREA    // Area enabled           : only the active area of the board is computed.
#endif // USEBITS_FAST_MT_AREA
```

| Level | Define this | `USEBITS` | `FAST` | `MT` | `AREA` |
|---:|---|:---:|:---:|:---:|:---:|
| 0 | (none: comment out `USEBITS_FAST_MT_AREA`) | - | - | - | - |
| 1 | `USEBITS_ONLY` | ✓ | - | - | - |
| 2 | `USEBITS_FAST` | ✓ | ✓ | - | - |
| 3 | `USEBITS_FAST_MT` | ✓ | ✓ | ✓ | - |
| 4 | `USEBITS_FAST_MT_AREA` (default) | ✓ | ✓ | ✓ | ✓ |

Define only one of them. If two are defined, the first one in the `#if` chain is used. The regression tests and the benchmark define `SHOS_LIFEGAME_CUSTOM_CONFIG`, and choose any of the 16 combinations from the command line.

### Overview

| `#define` | What it does | Trade-off |
|---|---|---|
| `USEBITS` | Stores one cell in one bit (`BitCellSet`). When undefined, each row is an array of `bool`. | Bit operations are harder to read than `bool` arrays. Reading a cell needs a shift and a mask. |
| `FAST` | Uses direct loops instead of a generic loop with `std::function` (`Utility::ForEach`). The next generation is computed row by row (`Game::NextRow`): the sum of the 3 cells in a column is shared by the neighboring cells. | Some code is duplicated. |
| `MT` | Splits the rows (Y direction) among the threads of `ThreadPool`, and each thread runs `NextPart`. The threads are kept for the whole game, and a small area is computed by fewer threads. | Less effective on small boards. |
| `AREA` | Computes only the active area: the bounding box of the live cells plus a margin of one cell. | Managing the area is complex. Less effective when the cells spread over the board. |

Besides these four, `BoardPainter` ([Shos.LifeGame/ShosLifeGameBoardPainter.h](Shos.LifeGame/ShosLifeGameBoardPainter.h)) optimizes drawing (see [Drawing](#drawing-boardpainter)).

Each technique below is explained in the same order:

- (a) The problem: what is slow without the optimization.
- (b) The idea.
- (c) The implementation: the functions and a short excerpt of the code.
- (d) The effect: measured values and their conditions.
- (e) The cost, and when the effect is small.
- (f) How it works with the other techniques.

**About the numbers.** Two kinds of measurements are used in this section. Do not compare one with the other.

- **Windows FPS** ([Benchmark](#benchmark)): generations per second in the title bar of the application, including drawing. They were measured before the speed-up work of October 2026.
- **WSL `Game::Next` time**: milliseconds per generation, measured with [Shos.LifeGame.Test/run_benchmark.sh](Shos.LifeGame.Test/run_benchmark.sh) (g++ 13.3, `-O2 -DNDEBUG`, WSL2, 8 threads, 1000 x 1000 board, no drawing). "Before" is the code before the speed-up work (Step 0), and "after" is the current code (Step 6). The values have an error of about ±20%, because WSL is disturbed by Windows. The details are in [Plans/2026-10-14_SpeedupPlan.md](Plans/2026-10-14_SpeedupPlan.md) (chapter 6).

### `USEBITS`: one bit per cell

**(a) The problem.** When `USEBITS` is undefined, `Board` stores each cell in a `bool`. A `bool` takes one byte, so a 1000 x 1000 board takes 1,000,000 bytes, and `Game` has two boards. The window cannot draw `bool` cells directly. So `Board::GetBits` converts all the 1,000,000 cells into a `BitCellSet` every time the board is painted.

**(b) The idea.** One cell is stored in one bit. The bits are laid out in the format of a monochrome (1bpp) bitmap, so that they can be drawn without converting them:

- The most significant bit (MSB) of each byte is the leftmost cell.
- Each row starts at a new byte, and the bytes of a row are aligned to a WORD (2 bytes).

```
x        :  0  1  2  3  4  5  6  7 |  8  9 10 11 12 13 14 15 | ... | 992 ... 999 | (padding)
byte     :  ------- byte 0 ------- |  ------- byte 1 ------- | ... |  byte 124   |  byte 125
bit      :  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 | ... |  7  ...  0  |  (unused)
```

A row of 1000 cells needs 125 bytes, and is padded to 126 bytes. A board takes 126,000 bytes, about 1/8 of the `bool` board.

**(c) The implementation.** `BitCellSet::ToIndex` maps a point to a byte and a bit. `BitCellSet::GetRowByteNumber` gives the bytes per row.

```cpp
static constexpr UnsignedInteger GetRowByteNumber(Integer width)
{ return UnsignedInteger((width + 15) / 16 * 2); }

bool ToIndex(const Point& point, UnsignedInteger& index, Byte& bit) const
{
    if (!GetRect().IsIn(point))
        return false;

    // The leftmost pixel is the most significant bit in a monochrome bitmap.
    // Each row starts at its own byte, so that threads computing different rows never write the same byte.
    constexpr auto bitNumber = sizeof(UnitInteger) * 8;
    index                    = UnsignedInteger(unitNumberX * point.y + point.x / bitNumber);
    bit                      = Byte(bitNumber - 1 - point.x % bitNumber);
    return true;
}
```

When `USEBITS` is defined, `Board` derives from `BitCellSet`, and `Board::GetBits` returns the bits as they are. When it is undefined, `Board` has the rows of `bool` and a `BitCellSet` only for painting.

**(d) The effect.**

- Windows FPS: Level 0 → 1 is 16 → 17 FPS (x1.06).
- WSL `Game::Next` (after, `FAST` defined): Random takes 4.951 ms with `bool` and 8.013 ms with bits. Gun_Gunstar takes 1.201 ms with `bool` and 3.571 ms with bits. The computation itself is slower with bits.

**(e) The cost.** Reading one cell needs a shift and a mask, and writing one cell needs a read-modify-write of a byte. So the computation is slower with bits, as the WSL values above show. The benefit of `USEBITS` is elsewhere: the memory is 1/8, and the board is drawn without the conversion. The WSL benchmark does not include drawing, so it does not show this benefit. Whether `USEBITS` is faster as a whole (computation plus drawing) after the speed-up work must be measured again on Windows.

**(f) With the other techniques.**

- With `FAST`, the cells are written one byte (8 cells) at a time by `BitCellSet::RowWriter`, and cleared by bytes in `BitCellSet::ClearRow`.
- With `MT`, each row starts at its own byte. So two threads that compute different rows never write the same byte.
- With `BoardPainter`, the bits are passed to `SetBitmapBits` as they are (see [Drawing](#drawing-boardpainter)).

### `FAST`: direct loops and row-by-row computation

**(a) The problem.** When `FAST` is undefined, the loops use `std::function` on purpose. `Utility::ForEach` calls an action through `std::function` for each cell of the board. For each cell, `GetAliveNeighborCount` calls `Utility::Count`, which calls another `std::function` for each of the 9 cells around it. Each `Get` checks that the point is inside the board. So one cell costs about 10 indirect calls, 9 bounds checks and 9 reads.

**(b) The idea.** `FAST` uses direct loops, and computes the next generation row by row. The rule needs the sum of the 3 x 3 cells around each cell. Two neighboring cells share two columns of their 3 x 3 cells. So the sum of the 3 cells in a column (the column sum) is computed once, and is used by 3 cells.

```
             x-1  x   x+1 x+2
  row y-1     a   b    c   d
  row y       e   f    g   h
  row y+1     i   j    k   l
             --- ---  --- ---
column sum    L   C    R   R'

cell x   : sum = L + C + R
cell x+1 : sum = C + R + R'   (C and R are reused: only R' is new)
```

The sum includes the cell itself. So the rule B3/S23 becomes: the cell is alive if the sum is 3, or if the sum is 4 and the cell is alive. For each cell, only the 3 cells of the new column are read, and the cell itself is read when the sum is 4.

The result is also written by rows. With `USEBITS`, `RowWriter` collects 8 cells in a register, and writes them to the byte at once.

**(c) The implementation.** `Game::NextRow` computes one row. The excerpt below omits the update of the area.

```cpp
const auto upperRow  = std::as_const(*mainBoard).GetRow(y - 1);
const auto row       = std::as_const(*mainBoard).GetRow(y    );
const auto lowerRow  = std::as_const(*mainBoard).GetRow(y + 1);
const auto columnSum = [&](Integer x) {
    return UnsignedInteger(Board::Get(upperRow, x)) + UnsignedInteger(Board::Get(row, x)) + UnsignedInteger(Board::Get(lowerRow, x));
};

Board::RowWriter writer(subBoard->GetRow(y), minimumX);
auto leftColumnSum   = minimumX > 0 ? columnSum(minimumX - 1) : 0U;
auto centerColumnSum = columnSum(minimumX);

const auto next = [&](Integer x, UnsignedInteger rightColumnSum) {
    const auto sum   = leftColumnSum + centerColumnSum + rightColumnSum;
    const auto alive = sum == 3 || (sum == 4 && Board::Get(row, x));
    writer.Write(alive);
    leftColumnSum   = centerColumnSum;
    centerColumnSum = rightColumnSum;
};
```

The cells outside the board are dead. `GetRow` returns a row of dead cells (`emptyRow`) for a row outside the board. The column on the right of the last cell is taken as 0. So the inner loop needs no bounds check.

```cpp
const auto innerMaximumX = std::min(maximumX, mainBoard->GetSize().cx - 1);
auto       x             = minimumX;
for (; x < innerMaximumX; x++)
    next(x, columnSum(x + 1));
if (x < maximumX)
    next(x, 0U);

writer.Flush();
```

`RowWriter::Flush` writes the last byte with a mask, so that the bits outside the computed range are kept.

**(d) The effect.**

- Windows FPS: Level 1 → 2 is 17 → 41 FPS (x2.41 from Level 1, x2.56 from Level 0). This was measured with the old `FAST`, which used direct loops but still read 8 neighbors for each cell.
- WSL `Game::Next`, before → after (the row-by-row computation, the byte writing and the update of the area by rows, Steps 1 to 3):

| Combination | Case | Before | After | Speed-up |
|---|---|---:|---:|---:|
| `FAST` | Random | 16.754 ms | 4.951 ms | x3.4 |
| `FAST` | Gun_Gunstar | 16.911 ms | 1.201 ms | x14.1 |
| `USEBITS` `FAST` | Random | 25.423 ms | 8.013 ms | x3.2 |
| `USEBITS` `FAST` | Gun_Gunstar | 18.971 ms | 3.571 ms | x5.3 |

**(e) The cost.** The code is longer and duplicated. Both `Board` classes must have the same row access (`GetRow`, `Get(row, x)`, `RowWriter` and `ClearRow`). The edges of the board need care: the rows outside, the last column, and the bytes at both ends of the range. The row-by-row computation is more effective with `bool` cells than with bits, because each read of a bit needs a shift and a mask.

**(f) With the other techniques.**

- With `USEBITS`, `RowWriter` writes 8 cells at once. Without it, `RowWriter` writes each `bool`.
- With `AREA`, `NextRow` also returns the first and the last live cells of the row. `NextPart` extends the area twice a row, instead of once for each live cell.
- With `MT`, the rows are independent, so each thread runs `NextRow` for its own rows.

### `MT`: splitting the rows among threads

**(a) The problem.** Without `MT`, one thread computes the whole range, and the other cores do nothing. The first version of `MT` (`ThreadUtility::ForEach`) created threads for each generation and joined them. Creating the threads took about 0.5 ms, which was longer than the computation of a small pattern. It also split even a small area among all the threads.

**(b) The idea.** The rows of the range are split into parts. Each thread computes its own part. The threads are created once with `Game`, and wait at a `std::barrier` for the work of each generation. The calling thread also works as thread 0.

```
rows of the range                       one generation
+--------------------+  thread 0        caller : set the work -> [start] -> part 0 -> [end] -> merge
|                    |  (caller)        thread 1:  wait ------> [start] -> part 1 -> [end] -> wait
+--------------------+  thread 1        thread 2:  wait ------> [start] -> part 2 -> [end] -> wait
|                    |                  ...
+--------------------+  ...             [start], [end]: barrier.arrive_and_wait()
|                    |
+--------------------+  thread 7
```

A small range is not worth waking threads up. So the number of parts is limited by the number of the cells per thread (`Game::minimumCellNumberPerThread`).

**(c) The implementation.** `ThreadPool::ForEach` decides the number of parts, and runs them.

```cpp
const auto size       = maximum - minimum;
const auto partNumber = unsigned(std::clamp(size / minimumSizePerThread, 1, Integer(threadNumber)));
if (partNumber == 1U) {
    action(minimum, maximum, 0U);
    return;
}
// ... stores the action, the range and partNumber ...
barrier.arrive_and_wait(); // Start
Run(0U);
barrier.arrive_and_wait(); // End
```

Each worker thread repeats the same two barriers.

```cpp
void Work(unsigned int threadIndex)
{
    for (;;) {
        barrier.arrive_and_wait(); // Start
        if (isStopping)
            return;
        Run(threadIndex);
        barrier.arrive_and_wait(); // End
    }
}
```

`Game::Next` converts the cells per thread into rows per thread: `std::max(1, minimumCellNumberPerThread / std::max(1, area.size.cx))`. The value is 65536 with `FAST`, and 1024 without it, because a cell costs much more without `FAST`. For example, on the whole 1000 x 1000 board, a thread needs at least 65 rows, so 8 threads are used on a CPU with 8 logical processors. An area of 82 x 140 cells is computed by the calling thread only.

**(d) The effect.**

- Windows FPS: Level 2 → 3 is 41 → 86 FPS (x2.10). This was measured with the old `MT`, which created threads every generation. The CPU has 4 cores and 8 logical processors.
- WSL `Game::Next` (after): Random takes 4.951 ms with `FAST` and 1.631 ms with `FAST` `MT` (about x3.0). It takes 8.013 ms with `USEBITS` `FAST` and 2.718 ms with `USEBITS` `FAST` `MT` (about x2.9).
- WSL `Game::Next`, before → after (the thread pool and the threshold, Step 4): Glider with the default combination is 0.475 ms → less than 0.0005 ms (more than x1000). Gun_Gunstar is 0.598 ms → 0.058 ms (x10.3). Before, most of the time was spent creating threads.
- The threshold was chosen by measuring 4096, 16384, 65536 and 262144 cells per thread. With smaller values, small patterns still woke threads up. With larger values, Random did not use enough threads.

**(e) The cost.** The threads must be synchronized, and the code must avoid data races. The speed-up is less than the number of threads: on the WSL machine with 8 threads, Random was about 3 times faster. Small boards and small areas gain nothing, because they are computed by one thread. The best threshold depends on the CPU. Drawing is not split among the threads.

**(f) With the other techniques.**

- With `AREA`, only the rows of the active area are split. The threshold uses the width of the area, so a narrow area needs more rows per thread.
- With `AREA`, the threads must not extend the same area at the same time. So each thread has its own area (`areas[index]`), and `Rect::Union` merges them after the end barrier. Each thread extends a local copy, and writes it to `areas[index]` once at the end. So the threads do not write the same cache line again and again (false sharing).
- With `USEBITS`, each row starts at its own byte, so the threads never write the same byte.

```cpp
std::ranges::fill(areas, BitCellSet::GetEmptyArea(mainBoard->GetSize()));

threadPool.ForEach(area.leftTop.y, areaRightBottom.y, minimumRowNumberPerThread, [=, this](Integer minimum, Integer maximum, unsigned int index) {
    NextPart(Point(area.leftTop.x, minimum), Point(areaRightBottom.x, maximum), areas[index]);
});

subBoard->SetArea(Rect::Union(areas));
```

### `AREA`: computing only the active area

**(a) The problem.** Without `AREA`, every generation computes all the 1,000,000 cells, even when most of them are dead. A dead cell with no live neighbors stays dead, so this work changes nothing.

**(b) The idea.** The board keeps an active area. The area is the bounding box of the live cells plus a margin of one cell, within the board. A cell outside the margin has no live neighbor, so it cannot be born. So only the area of the current generation is computed.

```
+---------------------------------------- board (1000 x 1000) ----+
|                                                                 |
|          +---------- area -----------+                          |
|          | . . . . . . . . . . . . . |  <- margin of one cell   |
|          | . # # . . . . . . . . . . |                          |
|          | . # . # . . . . . . # # . |                          |
|          | . . . . . . . . . . # . . |                          |
|          | . . . . . . . . . . . . . |                          |
|          +---------------------------+                          |
|                                                                 |
+-----------------------------------------------------------------+
```

The correctness depends on one invariant: **on each board, every cell outside the area is dead.**

The area of the next generation is computed again from an empty area, so it can shrink. When it shrinks, the board for the next generation (`subBoard`) still has old cells outside the new range. They are cleared before the computation.

```
generation n-1 (subBoard, area B)      generation n (mainBoard, area A)
+-------------------+                  +-------------------+
| B                 |                  |                   |
|    +--------+     |                  |    +--------+     |
|    | A      |     |   ClearOutside   |    | A      |     |
|    +--------+     |  ------------->  |    +--------+     |
| (old cells here)  |  (clear B - A)   |                   |
+-------------------+                  +-------------------+
Then the next generation is computed inside A, and written to subBoard.
```

**(c) The implementation.** `Game::Next` clears the old cells, starts from an empty area, and extends it by the live cells. If no cell is alive, the area goes back to the 3 x 3 cells at the center of the board (`GetDefaultArea`), so that it is never empty.

```cpp
// The area of subBoard is computed again from the cells alive in the next generation, so that it can shrink.
ClearOutside(*subBoard, mainBoard->GetArea());
subBoard->SetArea(BitCellSet::GetEmptyArea(subBoard->GetSize()));

// ... computes the next generation inside mainBoard->GetArea() ...

if (subBoard->GetArea().IsEmpty()) // No cells alive
    subBoard->SetArea(BitCellSet::GetDefaultArea(Rect(Point(), subBoard->GetSize())));
```

`Game::ClearOutside` visits only the rows of the old area, and clears the parts outside the new area. With `FAST`, `ClearRow` clears a row by bytes: it masks the bytes at both ends, and fills the bytes between them with 0.

`BitCellSet::Union` extends the area by a point and its neighbors, and clamps it to the board. With `FAST`, `NextPart` calls it twice a row, for the first and the last live cells.

```cpp
NextRow(y, minimum.x, maximum.x, firstAliveX, lastAliveX);
if (firstAliveX >= 0) {
    nextArea = BitCellSet::Union(nextArea, rect, Point(firstAliveX, y));
    nextArea = BitCellSet::Union(nextArea, rect, Point(lastAliveX , y));
}
```

In the Debug build (`_DEBUG`), `Game::Test` checks the invariant with `assert` every generation.

**(d) The effect.**

- Windows FPS: Level 3 → 4 is 86 → 244 FPS (x2.84).
- The area of Gun_Gunstar grows from 82 x 140 cells (generation 0) to 558 x 429 cells (generation 999). On average over 1000 generations, the area has about 99,000 cells, about 10% of the board. This was computed with the current code.
- The computation is reduced to about 1/10, but the FPS is only x2.84. Drawing and updating the title are done for the whole board, and `AREA` does not reduce them. This is probably the reason, but the time of drawing has not been measured on Windows.
- WSL `Game::Next` (after, `USEBITS` `FAST` `MT`): Random takes 2.718 ms without `AREA` and 3.060 ms with it. On a random board, `AREA` does not make it faster.
- WSL `Game::Next`, before → after (the update of the area by rows and the clearing by bytes, Step 3, together with Steps 1, 2 and 4): `FAST` `AREA` Gun_Gunstar is 0.156 ms → 0.017 ms (x9.2).

**(e) The cost, and when the effect is small.**

- The code for the area is complex. Every path must keep the invariant, or cells outside the area are left alive and the results are wrong. This is why the regression tests check all 16 combinations and the Debug build checks the invariant.
- A random board covers the whole board, so the area is the whole board from the start.
- A pattern that spreads over the board makes the area large. For example, the area of the pattern `Max` grows from 29 x 29 cells to 994 x 1000 cells in 1000 generations.
- A gun that keeps sending gliders makes the area large, because the area covers both the gun and the gliders. A single glider keeps a small area (5 x 5 cells), because the area moves with it.

**(f) With the other techniques.**

- With `MT`, each thread has its own area (see [`MT`](#mt-splitting-the-rows-among-threads)).
- With `FAST`, the area is extended twice a row, and the old cells are cleared by bytes.
- Without `FAST`, the area is extended for each live cell: by `Board::Set` without `MT`, and by `BitCellSet::Union` on a local area with `MT`.
- Without both `FAST` and `MT`, `Game::Next` visits the whole board (`mainBoard->ForEach` is called without `areaOnly`). So in this combination, `AREA` keeps the area up to date, but does not reduce the computation.

### Drawing (`BoardPainter`)

**(a) The problem.** The application draws the board after every generation. Before the speed-up work, `BoardPainter::Paint` created a bitmap and a memory device context for each paint, and deleted them after `BitBlt`. `MainWindow::Next` also called `SetWindowText` for every generation, which repaints the title bar.

**(b) The idea.**

- The bits of the board are copied to a monochrome bitmap without converting them, and the bitmap is drawn with `BitBlt`.
- The bitmap and the memory device context are created once, and reused while the size of the board stays the same.
- The title is updated at most once in 0.1 seconds.

```
Board::GetBits()  --SetBitmapBits-->  HBITMAP (kept)  --BitBlt-->  window
(1bpp layout,                          selected into a
 126 bytes x 1000 rows)                memory DC (kept)
```

**(c) The implementation.** `BoardPainter::Paint` creates the bitmap only when the size changes.

```cpp
const auto size = SIZE{ board.GetSize().cx, board.GetSize().cy };
if (bitmapHandle == nullptr || size.cx != bitmapSize.cx || size.cy != bitmapSize.cy) {
    Release();
    Create(deviceContextHandle, size);
}

// The bits of the board are copied without converting them.
// The size must agree with the layout of BitCellSet (BitCellSet::GetRowByteNumber and ToIndex).
::SetBitmapBits(bitmapHandle, DWORD(BitCellSet::GetRowByteNumber(size.cx)) * DWORD(size.cy), board.GetBits());
Paint(deviceContextHandle, position, size);
```

`Paint` selects the bitmap into the memory device context only while it is copied with `BitBlt`. `MainWindow::IsTitleToBeUpdated` decides when to update the title. The FPS is computed from the generation and the elapsed time, so it does not depend on when the title is updated.

**(d) The effect.** Not measured yet. Drawing cannot be measured on Linux, and the Windows application has not been measured after the speed-up work.

**(e) The cost.** The code depends on 1bpp bitmaps of GDI, so it is not portable. The bit layout of `BitCellSet` cannot be changed alone (see the test `BitmapMatchesBoard`). `SetDIBitsToDevice` was not used, because it needs rows aligned to a DWORD, which would change the layout of `BitCellSet`. Because the title is updated less often, the FPS of the current code cannot be compared with the FPS in [Benchmark](#benchmark).

**(f) With the other techniques.** With `USEBITS`, `GetBits` returns the bits of the board. Without `USEBITS`, `Board::GetBits` converts all the cells for each paint, by a direct loop with `FAST`, and by `Utility::ForEach` without it. `MT` and `AREA` do not change drawing: the whole board is drawn every time.

### Other small improvements

**`Game::Randomize`.** Before, `Randomize` used one 32-bit random number for one cell (`random.Next() % 2`). With `FAST`, one random number now gives 32 cells, one bit each. `Randomize` runs only when the board is filled at random, so it is not included in the benchmark.

```cpp
if (bitCount == 0) {
    bits     = random.Next();
    bitCount = 32;
}
mainBoard->Set(point, (bits & 1U) != 0U);
bits >>= 1;
bitCount--;
```

**`/arch:AVX2`.** It was tried with `-mavx2` of g++ on WSL. The default combination was 0.97 to 1.04 times as fast, so it was not added to the build settings.

### Summary of the speed-up work (WSL)

`Game::Next` of the default combination (`USEBITS` `FAST` `MT` `AREA`), before and after Steps 0 to 6:

| Case | Before | After | Speed-up |
|---|---:|---:|---:|
| Random | 11.682 ms | 3.060 ms | x3.8 |
| Glider | 0.475 ms | less than 0.0005 ms | more than x1000 |
| Gun_Gunstar | 0.598 ms | 0.058 ms | x10.3 |
| Max | 0.528 ms | 0.016 ms | x33.0 |

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
- **These values were measured before the speed-up work of October 2026** (see [Optimizations](#optimizations)). They must be measured again with the current code.

The raw data is in [Articles(Japanese)/ShosLifeGame_BenchmarkData.csv](Articles(Japanese)/ShosLifeGame_BenchmarkData.csv).

### Where each speed-up comes from

The values include both the computation and the drawing. The explanations below are about the code at the time of the measurement.

- **Level 0 → 1 (`USEBITS`, x1.06).** The computation still uses `std::function` for each cell and for each neighbor. Most of the time is probably spent in these calls. `USEBITS` removes the conversion of the `bool` cells into bits for each paint, and reduces the memory to 1/8. But the computation is not made faster, so the gain is small.
- **Level 1 → 2 (`FAST`, x2.41).** The `std::function` calls (about 10 for each cell) are replaced by direct loops. The bounds checks for each neighbor remained at that time.
- **Level 2 → 3 (`MT`, x2.10).** The rows are split among 8 threads, but the speed-up is much less than 8. The CPU has 4 cores. The threads were created and joined for every generation. Drawing and updating the title ran on one thread.
- **Level 3 → 4 (`AREA`, x2.84).** The active area of Gun_Gunstar is about 10% of the board on average, so the computation is about 1/10. The drawing of the whole board is not reduced, so the total gain is smaller (see [`AREA`](#area-computing-only-the-active-area)).

## Build and Run

Requirements: Windows, Visual Studio with the C++ desktop workload (platform toolset v145, C++20).

1. Open `Shos.LifeGame.sln` in Visual Studio.
2. Choose `Release` and `x64`, and build. The post-build event copies the `CellData` folder to the output folder.
3. Run `Shos.LifeGame`.

To compare the optimizations, choose a level at the top of `ShosLifeGame.h` (see [Optimizations](#optimizations)), rebuild, and check the FPS in the title bar. If you uncomment `#define LAP_TIMES 1000` in `Shos.LifeGame.cpp`, the title bar keeps showing the values at generation 1000, which is the condition of the benchmark.

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
