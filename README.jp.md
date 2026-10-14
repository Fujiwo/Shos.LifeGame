# Shos.LifeGame

[English](README.md) | [日本語](README.jp.md)

![LifeGame](Images/lifegame01.20240112.161130.gif)

[![LifeGame](Images/lifegame01.png)](https://youtu.be/ny-VKZEAVkg)

**C++20 と Win32 API だけで書いた高速な[ライフゲーム](https://ja.wikipedia.org/wiki/%E3%83%A9%E3%82%A4%E3%83%95%E3%82%B2%E3%83%BC%E3%83%A0) (B3/S23)。**

このリポジトリーは、最適化を学ぶための教材でもある。最適化の要素はそれぞれ `#define` で切り替えられるので、要素ごとにどれだけ速くなるかを測って比べられる。コードの解説は、日本語の記事 ([Articles(Japanese)/ShosLifeGame_Article.md](Articles(Japanese)/ShosLifeGame_Article.md)) にある。

## 特徴

- **4 つの最適化で約 15 倍速くなる**: 1000×1000 の盤面で、毎秒 16 世代から 244 世代になる ([ベンチマーク](#ベンチマーク)を参照)。この値は、2026 年 10 月の高速化の作業より前に Windows で測ったものであり、測り直す必要がある。
- **最適化の要素は `#define` で切り替えられる**: `ShosLifeGame.h` の冒頭でレベルを選んでビルドし直せば、タイトルバーの FPS で効果を比べられる。
- **盤面を変換せずに描画する**: `USEBITS` が定義されている場合、盤面のビット列を、そのまま 1bpp のビットマップとして `BitBlt` で描画する。
- **外部ライブラリーを使わない**: 使うのは C++20 の標準ライブラリーと Win32 API だけである。
- **16 通りの組み合わせをすべてテストする**: 回帰テストは、4 つの `#define` のすべての組み合わせを Linux/WSL でビルドして実行する。GitHub Actions でも同じテストを実行する。
- **有名なパターンを 46 個同梱している**: グライダー銃、宇宙船、パファー、メトセラなどを、Life 1.05 形式と RLE 形式で収録している。

## 最適化の要素

[Shos.LifeGame/ShosLifeGame.h](Shos.LifeGame/ShosLifeGame.h) の冒頭にある 4 つの `#define` が、最適化の中心である。4 つは、「レベル」を表す `#define` を 1 つ定義して選ぶ。初期状態では `USEBITS_FAST_MT_AREA` が定義されていて、4 つとも定義される。

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

| レベル | 定義するもの | `USEBITS` | `FAST` | `MT` | `AREA` |
|---:|---|:---:|:---:|:---:|:---:|
| 0 | なし (`USEBITS_FAST_MT_AREA` をコメントにする) | 未定義 | 未定義 | 未定義 | 未定義 |
| 1 | `USEBITS_ONLY` | 定義 | 未定義 | 未定義 | 未定義 |
| 2 | `USEBITS_FAST` | 定義 | 定義 | 未定義 | 未定義 |
| 3 | `USEBITS_FAST_MT` | 定義 | 定義 | 定義 | 未定義 |
| 4 | `USEBITS_FAST_MT_AREA` (初期状態) | 定義 | 定義 | 定義 | 定義 |

定義するのは 1 つだけにする。2 つ以上を定義すると、`#if` の並びで先にあるものが使われる。回帰テストとベンチマークは `SHOS_LIFEGAME_CUSTOM_CONFIG` を定義し、16 通りのどの組み合わせでもコマンドラインから選べるようにしている。

### 概要

| `#define` | 内容 | トレードオフ |
|---|---|---|
| `USEBITS` | 1 セルを 1 ビットで持つ (`BitCellSet`)。未定義の場合は、行ごとの `bool` 配列で持つ。 | ビット演算のコードは、`bool` 配列より読みにくい。セルを 1 つ読むたびに、シフトとマスクが要る。 |
| `FAST` | `std::function` を使う汎用ループ (`Utility::ForEach`) の代わりに、直接のループを使う。次の世代は行単位で計算し (`Game::NextRow`)、縦 3 セルの合計を隣のセルと共有する。 | 似たコードが重複する。 |
| `MT` | 行 (Y 方向) を `ThreadPool` のスレッドに分け、スレッドごとに `NextPart` を実行する。スレッドはゲームの間ずっと残しておき、範囲が小さいときは少ないスレッドで計算する。 | 小さい盤面では効果が出にくい。 |
| `AREA` | 活動範囲 (生きているセルの外接矩形に周囲 1 マスを加えた範囲) だけを計算する。 | 範囲の管理が複雑になる。セルが盤面全体に広がると効果が小さい。 |

この 4 つとは別に、`BoardPainter` ([Shos.LifeGame/ShosLifeGameBoardPainter.h](Shos.LifeGame/ShosLifeGameBoardPainter.h)) が描画を最適化している ([描画](#描画-boardpainter)を参照)。

以下では、それぞれの技法を同じ順で説明する。

- (a) 解決する問題: 最適化しない場合に、どこで何が遅いのか。
- (b) 考え方。
- (c) 実装: 該当する関数と、コードの要点。
- (d) 効果: 計測値と、その計測の条件。
- (e) 代償と、効果が小さくなる条件。
- (f) 他の技法との組み合わせ。

**数値について。** この節では、条件の違う 2 種類の計測値を使う。両者を比べてはいけない。

- **Windows の FPS** ([ベンチマーク](#ベンチマーク)): アプリのタイトルバーに出る、1 秒あたりの世代数。描画の時間を含む。2026 年 10 月の高速化の作業より前に測った値である。
- **WSL での `Game::Next` の時間**: 1 世代あたりのミリ秒。[Shos.LifeGame.Test/run_benchmark.sh](Shos.LifeGame.Test/run_benchmark.sh) で測った (g++ 13.3、`-O2 -DNDEBUG`、WSL2、8 スレッド、盤面 1000×1000、描画なし)。「着手前」は高速化の作業の前 (Step 0) のコード、「完了後」は現在 (Step 6) のコードである。WSL は Windows 側の負荷の影響を受けるので、±20% 程度の誤差がある。詳しくは [Plans/2026-10-14_SpeedupPlan.md](Plans/2026-10-14_SpeedupPlan.md) の 6 章にある。

### `USEBITS`: 1 セルを 1 ビットで持つ

**(a) 解決する問題。** `USEBITS` が未定義の場合、`Board` は 1 セルを 1 つの `bool` で持つ。`bool` は 1 バイトなので、1000×1000 の盤面は 1,000,000 バイトになる。`Game` はこの盤面を 2 枚持つ。また、`bool` のセルはそのままでは描画できない。そのため、`Board::GetBits` が描画のたびに 1,000,000 セルすべてを `BitCellSet` に変換している。

**(b) 考え方。** 1 セルを 1 ビットで持つ。ビットはモノクロ (1bpp) のビットマップと同じ配置に並べ、変換せずに描画できるようにする。

- 各バイトの最上位ビット (MSB) を、左端のセルにする。
- 各行は新しいバイトから始め、1 行のバイト数を WORD (2 バイト) 境界にそろえる。

```
x        :  0  1  2  3  4  5  6  7 |  8  9 10 11 12 13 14 15 | ... | 992 ... 999 | (詰め物)
バイト   :  ------ バイト 0 ------ |  ------ バイト 1 ------ | ... | バイト 124  | バイト 125
ビット   :  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 | ... |  7  ...  0  | (使わない)
```

1000 セルの行は 125 バイトに収まり、126 バイトに切り上げられる。盤面 1 枚は 126,000 バイトで、`bool` の盤面の約 1/8 になる。

**(c) 実装。** `BitCellSet::ToIndex` が、座標をバイトとビットの位置に変える。`BitCellSet::GetRowByteNumber` が、1 行のバイト数を返す。

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

`USEBITS` が定義されている場合、`Board` は `BitCellSet` を継承し、`Board::GetBits` はビット列をそのまま返す。未定義の場合、`Board` は `bool` の行と、描画のためだけの `BitCellSet` を持つ。

**(d) 効果。**

- Windows の FPS: レベル 0 → 1 で 16 → 17 FPS (1.06 倍)。
- WSL の `Game::Next` (完了後、`FAST` 定義): Random は、`bool` で 4.951 ミリ秒、ビットで 8.013 ミリ秒。Gun_Gunstar は、`bool` で 1.201 ミリ秒、ビットで 3.571 ミリ秒。計算そのものは、ビットのほうが遅い。

**(e) 代償。** セルを 1 つ読むにはシフトとマスクが、1 つ書くにはバイトの読み出しと書き戻しが要る。そのため、上の WSL の値のとおり、計算はビットのほうが遅い。`USEBITS` の利点は別のところにある。メモリーが 1/8 になることと、変換せずに描画できることである。WSL のベンチマークは描画を含まないので、この利点は数値に表れない。高速化の作業の後に、計算と描画を合わせて `USEBITS` のほうが速いかどうかは、Windows で測り直す必要がある。

**(f) 他の技法との組み合わせ。**

- `FAST` と組み合わせると、`BitCellSet::RowWriter` が 8 セル (1 バイト) ずつ書き込み、`BitCellSet::ClearRow` がバイト単位で消す。
- `MT` と組み合わせると、各行が別のバイトから始まることが効いてくる。別の行を計算する 2 つのスレッドが、同じバイトに書き込むことはない。
- `BoardPainter` は、ビット列をそのまま `SetBitmapBits` に渡す ([描画](#描画-boardpainter)を参照)。

### `FAST`: 直接のループと行単位の計算

**(a) 解決する問題。** `FAST` が未定義の場合、ループはあえて `std::function` を使って書いてある。`Utility::ForEach` は、盤面のセルごとに `std::function` 経由で処理を呼ぶ。セルごとに `GetAliveNeighborCount` が `Utility::Count` を呼び、周囲の 9 セルのそれぞれについて、さらに別の `std::function` を呼ぶ。`Get` は毎回、座標が盤面の中にあるかを調べる。つまり 1 セルあたり、約 10 回の間接呼び出し、9 回の範囲の確認、9 回の読み出しが要る。

**(b) 考え方。** `FAST` は直接のループを使い、次の世代を行単位で計算する。ルールの判定には、各セルの周囲 3×3 セルの合計が要る。隣り合う 2 つのセルは、3×3 のうち 2 列を共有している。そこで、縦 3 セルの合計 (列和) を 1 回だけ計算し、3 つのセルで使い回す。

```
             x-1  x   x+1 x+2
  行 y-1      a   b    c   d
  行 y        e   f    g   h
  行 y+1      i   j    k   l
             --- ---  --- ---
  列和        L   C    R   R'

セル x   : 合計 = L + C + R
セル x+1 : 合計 = C + R + R'   (C と R は使い回す。新しく計算するのは R' だけ)
```

この合計にはセル自身も含まれる。そのため、B3/S23 のルールは「合計が 3 なら生、合計が 4 で自身が生なら生」になる。1 セルごとに読むのは、新しい列の 3 セルと、合計が 4 のときのセル自身だけである。

結果も行単位で書き込む。`USEBITS` が定義されている場合、`RowWriter` は 8 セル分をレジスターに集めて、1 バイトをまとめて書き込む。

**(c) 実装。** `Game::NextRow` が 1 行を計算する。次の抜粋では、範囲の更新を省いている。

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

盤面の外のセルは死んでいるものとして扱う。盤面の外の行には、`GetRow` が死んだセルだけの行 (`emptyRow`) を返す。最後のセルの右の列は 0 とする。そのため、内側のループでは範囲を確認しなくてよい。

```cpp
const auto innerMaximumX = std::min(maximumX, mainBoard->GetSize().cx - 1);
auto       x             = minimumX;
for (; x < innerMaximumX; x++)
    next(x, columnSum(x + 1));
if (x < maximumX)
    next(x, 0U);

writer.Flush();
```

`RowWriter::Flush` は最後のバイトをマスクして書き込み、計算した範囲の外のビットを残す。

**(d) 効果。**

- Windows の FPS: レベル 1 → 2 で 17 → 41 FPS (レベル 1 から 2.41 倍、レベル 0 から 2.56 倍)。これは、以前の `FAST` で測った値である。以前の `FAST` は直接のループを使っていたが、セルごとに周囲の 8 セルを読んでいた。
- WSL の `Game::Next` の着手前 → 完了後 (行単位の計算、バイト単位の書き込み、行単位の範囲の更新。Step 1〜3):

| 組み合わせ | ケース | 着手前 | 完了後 | 倍率 |
|---|---|---:|---:|---:|
| `FAST` | Random | 16.754 ミリ秒 | 4.951 ミリ秒 | 3.4 倍 |
| `FAST` | Gun_Gunstar | 16.911 ミリ秒 | 1.201 ミリ秒 | 14.1 倍 |
| `USEBITS` `FAST` | Random | 25.423 ミリ秒 | 8.013 ミリ秒 | 3.2 倍 |
| `USEBITS` `FAST` | Gun_Gunstar | 18.971 ミリ秒 | 3.571 ミリ秒 | 5.3 倍 |

**(e) 代償。** コードが長くなり、似たコードが重複する。2 つの `Board` は、同じ形の行アクセス (`GetRow`、`Get(row, x)`、`RowWriter`、`ClearRow`) を持たなければならない。盤面の端にも注意が要る。盤面の外の行、最後の列、範囲の両端のバイトである。行単位の計算は、ビットのセルより `bool` のセルのほうがよく効く。ビットは読むたびにシフトとマスクが要るからである。

**(f) 他の技法との組み合わせ。**

- `USEBITS` と組み合わせると、`RowWriter` が 8 セルをまとめて書き込む。未定義の場合、`RowWriter` は `bool` を 1 つずつ書き込む。
- `AREA` と組み合わせると、`NextRow` は、その行で最初と最後に生きているセルの x も返す。`NextPart` は、生きているセルごとではなく、1 行に 2 回だけ範囲を広げる。
- `MT` と組み合わせると、行どうしが独立していることが効いてくる。各スレッドは、自分の行について `NextRow` を実行する。

### `MT`: 行をスレッドに分ける

**(a) 解決する問題。** `MT` が未定義の場合、1 つのスレッドがすべての範囲を計算し、ほかのコアは何もしない。また、以前の `MT` (`ThreadUtility::ForEach`) は、世代ごとにスレッドを作って join していた。スレッドの生成には約 0.5 ミリ秒かかり、小さなパターンでは計算そのものより長かった。しかも、小さな範囲でも、すべてのスレッドに分けていた。

**(b) 考え方。** 計算する範囲を行で分け、各スレッドが自分の部分を計算する。スレッドは `Game` の生成時に 1 回だけ作り、世代ごとの仕事を `std::barrier` で待つ。呼び出し元のスレッドも、スレッド 0 として計算に加わる。

```
計算する範囲の行                         1 世代の流れ
+--------------------+  スレッド 0       呼び出し元 : 仕事を設定 -> [開始] -> 部分 0 -> [終了] -> 範囲をまとめる
|                    |  (呼び出し元)     スレッド 1 : 待機 -----> [開始] -> 部分 1 -> [終了] -> 待機
+--------------------+  スレッド 1       スレッド 2 : 待機 -----> [開始] -> 部分 2 -> [終了] -> 待機
|                    |                   ...
+--------------------+  ...              [開始]、[終了]: barrier.arrive_and_wait()
|                    |
+--------------------+  スレッド 7
```

範囲が小さいと、スレッドを起こす費用のほうが高くつく。そこで、1 スレッドあたりのセル数 (`Game::minimumCellNumberPerThread`) で、分ける数を制限する。

**(c) 実装。** `ThreadPool::ForEach` が分ける数を決め、各部分を実行する。

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

ワーカースレッドは、同じ 2 つの `barrier` を繰り返す。

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

`Game::Next` は、1 スレッドあたりのセル数を行数に直す (`std::max(1, minimumCellNumberPerThread / std::max(1, area.size.cx))`)。この値は、`FAST` が定義されていれば 65536、未定義なら 1024 である。`FAST` が未定義だと、1 セルの計算がずっと重いからである。たとえば 1000×1000 の盤面全体では、1 スレッドに 65 行以上が要るので、論理プロセッサーが 8 個の CPU では 8 スレッドを使う。82×140 セルの範囲は、呼び出し元のスレッドだけで計算する。

**(d) 効果。**

- Windows の FPS: レベル 2 → 3 で 41 → 86 FPS (2.10 倍)。これは、世代ごとにスレッドを作っていた以前の `MT` で測った値である。CPU は 4 コア、8 論理プロセッサーである。
- WSL の `Game::Next` (完了後): Random は、`FAST` で 4.951 ミリ秒、`FAST` `MT` で 1.631 ミリ秒 (約 3.0 倍)。`USEBITS` `FAST` で 8.013 ミリ秒、`USEBITS` `FAST` `MT` で 2.718 ミリ秒 (約 2.9 倍)。
- WSL の `Game::Next` の着手前 → 完了後 (スレッドの常駐化と分割のしきい値。Step 4): 既定の組み合わせの Glider は、0.475 ミリ秒から 0.0005 ミリ秒未満になった (1000 倍以上)。Gun_Gunstar は 0.598 → 0.058 ミリ秒 (10.3 倍)。着手前は、時間の大部分をスレッドの生成に使っていた。
- しきい値は、1 スレッドあたり 4096、16384、65536、262144 セルで測って決めた。小さい値では、小さなパターンでもスレッドを起こしてしまった。大きい値では、Random でスレッドが足りなくなった。

**(e) 代償。** スレッドの同期が要り、データ競合を避けるようにコードを書かなければならない。速さはスレッドの数ほどには伸びない。8 スレッドの WSL の環境で、Random は約 3 倍だった。小さな盤面や小さな範囲は 1 スレッドで計算するので、速くならない。最適なしきい値は CPU によって変わる。描画はスレッドに分けていない。

**(f) 他の技法との組み合わせ。**

- `AREA` と組み合わせると、分けるのは活動範囲の行だけになる。しきい値の計算には範囲の幅を使うので、幅の狭い範囲では 1 スレッドあたりの行数が増える。
- `AREA` と組み合わせる場合、複数のスレッドが同じ範囲を同時に広げてはいけない。そこで、スレッドごとに範囲 (`areas[index]`) を持ち、終了の `barrier` の後に `Rect::Union` でまとめる。各スレッドはローカル変数の範囲を広げ、最後に 1 回だけ `areas[index]` に書き込む。こうすると、複数のスレッドが同じキャッシュラインに何度も書き込むこと (false sharing) が起きない。
- `USEBITS` と組み合わせても、各行が別のバイトから始まるので、スレッドどうしが同じバイトに書き込むことはない。

```cpp
std::ranges::fill(areas, BitCellSet::GetEmptyArea(mainBoard->GetSize()));

threadPool.ForEach(area.leftTop.y, areaRightBottom.y, minimumRowNumberPerThread, [=, this](Integer minimum, Integer maximum, unsigned int index) {
    NextPart(Point(area.leftTop.x, minimum), Point(areaRightBottom.x, maximum), areas[index]);
});

subBoard->SetArea(Rect::Union(areas));
```

### `AREA`: 活動範囲だけを計算する

**(a) 解決する問題。** `AREA` が未定義の場合、ほとんどのセルが死んでいても、毎世代 1,000,000 セルすべてを計算する。周囲に生きているセルがない死んだセルは死んだままなので、この計算は何も変えない。

**(b) 考え方。** 盤面は活動範囲を持つ。活動範囲は、生きているセルの外接矩形に周囲 1 マスを加え、盤面の中に収めたものである。周囲 1 マスより外のセルは、生きている隣のセルを持たないので、誕生できない。そのため、計算するのは現在の世代の活動範囲だけでよい。

```
+------------------------------------------- 盤面 (1000×1000) ----+
|                                                                 |
|          +-------- 活動範囲 ---------+                          |
|          | . . . . . . . . . . . . . |  <- 周囲 1 マス          |
|          | . # # . . . . . . . . . . |                          |
|          | . # . # . . . . . . # # . |                          |
|          | . . . . . . . . . . # . . |                          |
|          | . . . . . . . . . . . . . |                          |
|          +---------------------------+                          |
|                                                                 |
+-----------------------------------------------------------------+
```

正しさは、1 つの不変条件で支えられている。**「各盤面で、範囲の外のセルはすべて死んでいる」**。

次の世代の範囲は、空の範囲から計算し直す。そのため、範囲は縮むこともある。範囲が縮むと、次の世代を書き込む盤面 (`subBoard`) には、新しい範囲の外に古いセルが残っている。これを計算の前に消す。

```
世代 n-1 (subBoard、範囲 B)            世代 n (mainBoard、範囲 A)
+-------------------+                  +-------------------+
| B                 |                  |                   |
|    +--------+     |                  |    +--------+     |
|    | A      |     |   ClearOutside   |    | A      |     |
|    +--------+     |  ------------->  |    +--------+     |
| (古いセルが残る)  |  (B のうち A の  |                   |
+-------------------+   外を消す)      +-------------------+
この後、A の中で次の世代を計算し、subBoard に書き込む。
```

**(c) 実装。** `Game::Next` は、古いセルを消し、空の範囲から始めて、生きているセルで範囲を広げる。生きているセルが 1 つもなければ、範囲を盤面中央の 3×3 (`GetDefaultArea`) に戻し、範囲が空にならないようにする。

```cpp
// The area of subBoard is computed again from the cells alive in the next generation, so that it can shrink.
ClearOutside(*subBoard, mainBoard->GetArea());
subBoard->SetArea(BitCellSet::GetEmptyArea(subBoard->GetSize()));

// ... computes the next generation inside mainBoard->GetArea() ...

if (subBoard->GetArea().IsEmpty()) // No cells alive
    subBoard->SetArea(BitCellSet::GetDefaultArea(Rect(Point(), subBoard->GetSize())));
```

`Game::ClearOutside` は、古い範囲の行だけを調べ、新しい範囲の外の部分を消す。`FAST` が定義されていれば、`ClearRow` がバイト単位で消す。両端のバイトはマスクをかけ、間のバイトはまとめて 0 にする。

`BitCellSet::Union` は、点とその周囲を含むように範囲を広げ、盤面の中に収める。`FAST` が定義されていれば、`NextPart` は 1 行に 2 回、最初と最後に生きているセルで呼ぶ。

```cpp
NextRow(y, minimum.x, maximum.x, firstAliveX, lastAliveX);
if (firstAliveX >= 0) {
    nextArea = BitCellSet::Union(nextArea, rect, Point(firstAliveX, y));
    nextArea = BitCellSet::Union(nextArea, rect, Point(lastAliveX , y));
}
```

Debug ビルド (`_DEBUG`) では、`Game::Test` が毎世代この不変条件を `assert` で確かめる。

**(d) 効果。**

- Windows の FPS: レベル 3 → 4 で 86 → 244 FPS (2.84 倍)。
- Gun_Gunstar の活動範囲は、82×140 セル (0 世代目) から 558×429 セル (999 世代目) に広がる。1000 世代の平均は約 99,000 セルで、盤面の約 10% である。この値は、現在のコードで計算した。
- 計算量は約 1/10 になるのに、FPS は 2.84 倍にとどまる。描画とタイトルの更新は盤面全体に対して行うので、`AREA` では減らない。これが理由だと考えられるが、Windows で描画の時間は測っていない。
- WSL の `Game::Next` (完了後、`USEBITS` `FAST` `MT`): Random は、`AREA` なしで 2.718 ミリ秒、`AREA` ありで 3.060 ミリ秒。ランダムな盤面では、`AREA` を加えても速くならない。
- WSL の `Game::Next` の着手前 → 完了後 (行単位の範囲の更新とバイト単位の消去。Step 3 と、Step 1、2、4 の合計): `FAST` `AREA` の Gun_Gunstar は 0.156 → 0.017 ミリ秒 (9.2 倍)。

**(e) 代償と、効果が小さくなる条件。**

- 範囲を管理するコードは複雑である。どの経路でも不変条件を守らないと、範囲の外に生きたセルが残り、結果が狂う。回帰テストが 16 通りすべてを確かめ、Debug ビルドが不変条件を確かめているのは、このためである。
- ランダムな盤面は、最初から盤面全体が範囲になる。
- 盤面全体に広がるパターンでは、範囲が大きくなる。たとえばパターン `Max` の範囲は、1000 世代で 29×29 セルから 994×1000 セルに広がる。
- グライダーを撃ち続ける銃では、範囲が大きくなる。範囲が銃とグライダーの両方を含むからである。グライダー 1 つだけなら、範囲はグライダーと一緒に動くので、5×5 セルのままである。

**(f) 他の技法との組み合わせ。**

- `MT` と組み合わせると、スレッドごとに範囲を持つ ([`MT`](#mt-行をスレッドに分ける)を参照)。
- `FAST` と組み合わせると、範囲は 1 行に 2 回だけ広げ、古いセルはバイト単位で消す。
- `FAST` が未定義の場合、範囲は生きているセルごとに広げる。`MT` が未定義なら `Board::Set` で、定義されていればローカル変数の範囲に対する `BitCellSet::Union` で広げる。
- `FAST` と `MT` がどちらも未定義の場合、`Game::Next` は盤面全体を調べる (`mainBoard->ForEach` を `areaOnly` なしで呼んでいる)。そのため、この組み合わせの `AREA` は範囲を更新するだけで、計算量は減らない。

### 描画 (`BoardPainter`)

**(a) 解決する問題。** アプリは、世代ごとに盤面を描画する。高速化の作業の前は、`BoardPainter::Paint` が描画のたびにビットマップとメモリー デバイス コンテキストを作り、`BitBlt` の後で削除していた。また、`MainWindow::Next` は世代ごとに `SetWindowText` を呼び、タイトルバーを再描画させていた。

**(b) 考え方。**

- 盤面のビット列を変換せずにモノクロのビットマップへ写し、`BitBlt` で描画する。
- ビットマップとメモリー デバイス コンテキストは 1 回だけ作り、盤面の大きさが変わらない間は使い回す。
- タイトルの更新は、0.1 秒に 1 回までにする。

```
Board::GetBits()  --SetBitmapBits-->  HBITMAP (保持)  --BitBlt-->  ウィンドウ
(1bpp の配置、                         メモリー DC (保持)
 126 バイト×1000 行)                   に選択して転送
```

**(c) 実装。** `BoardPainter::Paint` は、大きさが変わったときだけビットマップを作る。

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

`Paint` は、`BitBlt` で写す間だけ、ビットマップをメモリー デバイス コンテキストに選択する。タイトルを更新するかどうかは、`MainWindow::IsTitleToBeUpdated` が決める。FPS は世代数と経過時間から計算するので、タイトルをいつ更新しても値は変わらない。

**(d) 効果。** まだ測っていない。描画は Linux では測れず、高速化の作業の後に Windows のアプリを測っていないからである。

**(e) 代償。** GDI の 1bpp のビットマップに依存するので、ほかの環境へは移しにくい。`BitCellSet` のビット配置は、描画と切り離して変えられない (テスト `BitmapMatchesBoard` を参照)。`SetDIBitsToDevice` は使わなかった。行を DWORD 境界にそろえる必要があり、`BitCellSet` の配置が変わってしまうからである。また、タイトルの更新を減らしたので、現在のコードの FPS は、[ベンチマーク](#ベンチマーク)の FPS とは比べられない。

**(f) 他の技法との組み合わせ。** `USEBITS` が定義されていれば、`GetBits` は盤面のビット列を返す。未定義の場合は、`Board::GetBits` が描画のたびにすべてのセルを変換する。`FAST` が定義されていれば直接のループで、未定義なら `Utility::ForEach` で変換する。`MT` と `AREA` は描画を変えない。毎回、盤面全体を描画する。

### その他の小さな改善

**`Game::Randomize`。** 以前の `Randomize` は、32 ビットの乱数 1 つを 1 セルに使っていた (`random.Next() % 2`)。現在は、`FAST` が定義されていれば、1 つの乱数の 32 ビットを 1 ビットずつ 32 セルに使う。`Randomize` は盤面をランダムに埋めるときにしか呼ばれないので、ベンチマークの対象外である。

```cpp
if (bitCount == 0) {
    bits     = random.Next();
    bitCount = 32;
}
mainBoard->Set(point, (bits & 1U) != 0U);
bits >>= 1;
bitCount--;
```

**`/arch:AVX2`。** WSL の g++ の `-mavx2` で試した。既定の組み合わせで 0.97〜1.04 倍と差がなかったので、ビルド設定には加えなかった。

### 高速化の作業のまとめ (WSL)

既定の組み合わせ (`USEBITS` `FAST` `MT` `AREA`) の `Game::Next` の、Step 0〜6 の着手前と完了後:

| ケース | 着手前 | 完了後 | 倍率 |
|---|---:|---:|---:|
| Random | 11.682 ミリ秒 | 3.060 ミリ秒 | 3.8 倍 |
| Glider | 0.475 ミリ秒 | 0.0005 ミリ秒未満 | 1000 倍以上 |
| Gun_Gunstar | 0.598 ミリ秒 | 0.058 ミリ秒 | 10.3 倍 |
| Max | 0.528 ミリ秒 | 0.016 ミリ秒 | 33.0 倍 |

## ベンチマーク

各レベルは、前のレベルに最適化の要素を 1 つずつ加えたものである。

| レベル | `USEBITS` | `FAST` | `MT` | `AREA` | 実行時間 | 世代/秒 | 倍率 |
|---:|:---:|:---:|:---:|:---:|---:|---:|---:|
| 0 | 未定義 | 未定義 | 未定義 | 未定義 | 63.8 秒 | 16 | 1.00 |
| 1 | 定義 | 未定義 | 未定義 | 未定義 | 60.2 秒 | 17 | 1.06 |
| 2 | 定義 | 定義 | 未定義 | 未定義 | 24.2 秒 | 41 | 2.56 |
| 3 | 定義 | 定義 | 定義 | 未定義 | 11.7 秒 | 86 | 5.38 |
| 4 | 定義 | 定義 | 定義 | 定義 | 4.1 秒 | 244 | 15.25 |

- 測定条件: パターン `Gun_Gunstar`、盤面 1000×1000、1000 世代、Release ビルド。
- 測定環境: DELL XPS 13 9310、11th Gen Intel Core i7 2.92GHz (4 コア、8 論理プロセッサー)、メモリー 16GB、Windows 11。
- 結果は、CPU、ビルド設定、パターンによって変わる。たとえばランダムな盤面では、活動範囲が盤面全体になるので、`AREA` の効果は小さい。
- **この値は、2026 年 10 月の高速化の作業より前に測ったものである** ([最適化の要素](#最適化の要素)を参照)。現在のコードで測り直す必要がある。

測定データは [Articles(Japanese)/ShosLifeGame_BenchmarkData.csv](Articles(Japanese)/ShosLifeGame_BenchmarkData.csv) にある。

### 各レベルの伸びの理由

この値は、計算と描画の両方を含む。以下は、測定した時点のコードについての説明である。

- **レベル 0 → 1 (`USEBITS`、1.06 倍)。** 計算には、まだセルごと、隣のセルごとに `std::function` を使っている。時間の大部分は、この呼び出しに使われていると考えられる。`USEBITS` によって、描画のたびの `bool` からビットへの変換がなくなり、メモリーも 1/8 になる。しかし計算は速くならないので、伸びは小さい。
- **レベル 1 → 2 (`FAST`、2.41 倍)。** セルごとに約 10 回の `std::function` の呼び出しが、直接のループに置き換わる。隣のセルごとの範囲の確認は、この時点ではまだ残っていた。
- **レベル 2 → 3 (`MT`、2.10 倍)。** 行を 8 スレッドに分けたが、伸びは 8 倍よりずっと小さい。CPU のコアは 4 つである。スレッドは世代ごとに作って join していた。描画とタイトルの更新は、1 つのスレッドで実行していた。
- **レベル 3 → 4 (`AREA`、2.84 倍)。** Gun_Gunstar の活動範囲は平均で盤面の約 10% なので、計算量は約 1/10 になる。盤面全体の描画は減らないので、全体の伸びはこれより小さい ([`AREA`](#area-活動範囲だけを計算する)を参照)。

## ビルドと実行

必要なもの: Windows、C++ によるデスクトップ開発のワークロードを入れた Visual Studio (プラットフォーム ツールセット v145、C++20)。

1. Visual Studio で `Shos.LifeGame.sln` を開く。
2. `Release` と `x64` を選んでビルドする。ビルド後のイベントで、`CellData` フォルダーが出力先にコピーされる。
3. `Shos.LifeGame` を実行する。

最適化の効果を比べるには、`ShosLifeGame.h` の冒頭でレベルを選び ([最適化の要素](#最適化の要素)を参照)、ビルドし直して、タイトルバーの FPS を見る。`Shos.LifeGame.cpp` の `#define LAP_TIMES 1000` のコメントを外すと、タイトルバーには 1000 世代の時点の値が表示され続ける。これはベンチマークの測定条件と同じである。

## 使い方

| 操作 | 動作 |
|---|---|
| `0`–`9`、`a`–`z`、`A`–`J` | `CellData` のパターンを読み込む |
| それ以外のキー (`K`–`Z` など) | 盤面をランダムに埋める |
| 右クリック | 盤面をランダムに埋める |

- 盤面は 1000×1000 セルである。盤面の外のセルは常に死んでいる (盤面はトーラスではない)。
- タイトルバーには、世代、経過時間、FPS (1 秒あたりの世代数)、パターン名が表示される。
- メッセージキューが空になるたびに 1 世代を進める。そのため CPU を 1 コア使い続けるが、これは最大の FPS を測るための意図した動作である。

## パターンファイル

パターンファイルは、実行ファイルと同じ場所にある `CellData` フォルダーから読み込む。対応している形式は次の 2 つである (拡張子の大文字と小文字は区別しない)。

- [Life 1.05](https://conwaylife.com/wiki/Life_1.05) (`.lif`/`.life`): `#` 行と `!` で始まるコメント行は読み飛ばす。`#P x y` によるブロックの配置に対応している。
- [RLE](https://conwaylife.com/wiki/Run_Length_Encoded) (`.rle`): ヘッダーの `rule` は無視する (常に B3/S23 を使う)。

ファイルは、Life 1.05 形式が先、RLE 形式が後の順に並べる。それぞれの中では、ファイル名を大文字と小文字を区別せずに並べる。この並び順に、キー `0`–`9`、`a`–`z`、`A`–`J` が割り当てられる。ファイルを追加したり名前を変えたりすると、キーの割り当てがずれる。

## テスト

[Shos.LifeGame.Test/Shos.LifeGame.RegressionTest.cpp](Shos.LifeGame.Test/Shos.LifeGame.RegressionTest.cpp) には、シミュレーション、活動範囲、ビットマップのビット配置、パターンファイルの読み込みの回帰テストがある。シミュレーションの結果は、単純な参照実装と比べて検証する。次のスクリプトは、`USEBITS`/`FAST`/`MT`/`AREA` の 16 通りの組み合わせすべてを、Linux または WSL の g++ でビルドして実行する。GitHub Actions も同じスクリプトを実行する ([.github/workflows/regression-tests.yml](.github/workflows/regression-tests.yml))。

```
./Shos.LifeGame.Test/run_regression_tests.sh          # すべての組み合わせ (約 2 分)
./Shos.LifeGame.Test/run_regression_tests.sh --asan   # AddressSanitizer と UndefinedBehaviorSanitizer 付き
```

[Shos.LifeGame.Test/Shos.LifeGame.Benchmark.cpp](Shos.LifeGame.Test/Shos.LifeGame.Benchmark.cpp) は、ウィンドウを使わずに `Game::Next` の速さを測る。次のスクリプトは、これを `-O2` でビルドし、組み合わせを 1 つずつ実行する。各ケースは 3 回実行し、最も速い値を表示する。

```
./Shos.LifeGame.Test/run_benchmark.sh                        # すべての組み合わせ
./Shos.LifeGame.Test/run_benchmark.sh --scale 0.1 FAST_MT_AREA USEBITS_FAST_MT_AREA   # 世代数を減らし、一部の組み合わせだけ
```

`Shos.LifeGame.Test/Shos.LifeGame.Test.cpp` (Visual Studio のテストプロジェクト) はベンチマークであり、結果の正しさは検証しない。

## フォルダー構成

```
Shos.LifeGame/                 アプリケーション
  ShosLifeGame.h               ゲームのロジック、盤面、パターン、最適化 (プラットフォームに依存しない)
  ShosLifeGameBoardPainter.h   盤面の描画 (Win32 GDI)
  ShosWin32.h                  ウィンドウとメッセージループの小さなフレームワーク
  ShosHelper.h                 コレクション、ファイル、文字列のヘルパー
  ShosStopwatch.h              ストップウォッチ
  Shos.LifeGame.cpp            メインウィンドウとエントリーポイント
  CellData/                    パターンファイル (.lif/.rle)
Shos.LifeGame.Test/            回帰テストとベンチマーク (g++)、ベンチマーク (Visual Studio)
Articles(Japanese)/            日本語の記事、図、ベンチマークのデータ
Images/                        スクリーンショットとアニメーション
```

## プログラムの構成

### namespace Shos::LifeGame (`ShosLifeGame.h`)

- `Random`: `std::random_device` と `std::mt19937` で乱数を作る。
- `Size`、`Point`、`Rect`: 盤面上の大きさ、座標、矩形を表す。
- `Utility`: 矩形内の各点を回す汎用ループ (`FAST` が未定義のときに使う)。
- `ThreadPool`: ゲームの間ずっと残しておくスレッドで、整数の範囲を分けて処理する (`MT` が定義されているときに使う)。
- `Pattern`、`PatternSet`: 初期パターンを表し、Life 1.05 形式と RLE 形式のファイルから読み込む。
- `BitCellSet`: 1 セルを 1 ビットで持つセルの集合。ビット配置は 1bpp のビットマップに合わせてある。
- `Board`: 盤面。`USEBITS` が定義されていれば `BitCellSet` を継承し、未定義なら行ごとの `bool` 配列を使う。
- `Game`: 現在の盤面から次の世代を計算し、活動範囲を管理する。

### namespace Shos::LifeGame::Win32 (`ShosLifeGameBoardPainter.h`)

- `BoardPainter`: 盤面をデバイスコンテキストに描画する。描画の最適化も受け持つ。

### namespace Shos::LifeGame::Application (`Shos.LifeGame.cpp`)

- `MainWindow`: メインウィンドウ。キーとマウスの操作を受け付け、盤面と FPS を表示する。
- `Program`: メインウィンドウを作り、メッセージループを実行する。

### namespace Shos::Win32 (`ShosWin32.h`)

- `Window`: ウィンドウの基底クラス。メッセージを仮想関数 `On...` に振り分ける。
- `Program`: アプリケーションの基底クラス。メッセージループを実行する。

### namespace Shos (`ShosHelper.h`、`ShosStopwatch.h`)

- `Helper`: コレクション用の汎用ヘルパー (`ForEach`、`Map`、`Filter`、`Maximum`、`Connect`)。
- `File`: フォルダー内のファイルの一覧を取得し、テキストファイルを読み込む。
- `String`: 文字列のトリム、パディング、分割、小文字への変換を行う。
- `stopwatch`: `std::chrono::steady_clock` で時間を測る。

## 解説記事

- [C++/Win32で学ぶ高速ライフゲーム実装: Shos.LifeGame徹底解説](Articles(Japanese)/ShosLifeGame_Article.md)

ライフゲームのルール、パターンファイルの形式、データ構造、最適化の各要素、ベンチマークを、図を使って解説している。

## 作者

小島 富治雄 (Fujio Kojima): 日本のソフトウェア開発者
* Microsoft MVP for Development Tools - Visual C# (2005 年 7 月 - 2014 年 12 月)
* Microsoft MVP for .NET (2015 年 1 月 - 2015 年 10 月)
* Microsoft MVP for Visual Studio and Development Technologies (2015 年 11 月 - 2018 年 6 月)
* Microsoft MVP for Developer Technologies (2018 年 11 月 - 2026 年 6 月)
* [MVP Profile](https://mvp.microsoft.com/en-US/mvp/profile/4185d172-3c9a-e411-93f2-9cb65495d3c4 "MVP Profile")
* [ブログ](http://wp.shos.info "ブログ")
* [Web サイト](http://www.shos.info "Web サイト")
* [Twitter](https://twitter.com/Fujiwo)
* [Instagram](https://www.instagram.com/fujiwo/)

## ライセンス

このプロジェクトは MIT License で公開している。詳しくは [LICENSE.txt](LICENSE.txt) を参照。
