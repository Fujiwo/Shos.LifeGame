# 既存コード高速化 計画書

- 作成日: 2026-10-14
- 対象: `Shos.LifeGame/ShosLifeGame.h`、`Shos.LifeGame/ShosLifeGameBoardPainter.h`、`Shos.LifeGame/Shos.LifeGame.cpp`
- 範囲: 新しい `#define` オプションを追加せずに行う高速化

## 1. 目的と方針

既存の 4 オプション（`USEBITS` / `FAST` / `MT` / `AREA`）の枠組みを保ったまま、各オプションが有効なときの処理を速くする。

方針は次のとおり。

- **オプションの意味を変えない。** README のとおり、このプログラムは「オプションを 1 つずつ有効にすると速くなる」ことを示す教材でもある。改善は、それぞれ対応するオプションが有効な経路に入れる。`FAST` 無効時の `std::function` を使う汎用ループのように、あえて遅く書いてある経路には手を入れない。
- **16 通りの組み合わせすべてで、結果を変えない。** `Shos.LifeGame.Test/run_regression_tests.sh` が全組み合わせで通ることを、各ステップの完了条件にする。
- **`AREA` の不変条件を守る。** 「area の外側に生存セルはない」「area は生存セルの周囲 1 セルを含み、ボードの範囲内に収まる」という条件を、`_DEBUG` 時の `Game::Test` で確認し続ける。
- **計測してから進める。** 各ステップの前後でベンチマークを取り、効果がなかった変更は取り消す。

## 2. 現状の主なボトルネック

| # | 箇所 | 問題 |
|---|------|------|
| A | `Board::GetAliveNeighborCount` | 1 セルにつき `Get()` を 8 回呼ぶ。`Get()` のたびに `IsIn` の境界チェック、index 計算、ビットシフトを行う |
| B | `Game::NextPart` | 結果を `SetOnly` で 1 セル（1 ビット）ずつ read-modify-write している |
| C | `Game::NextPart`（`AREA` && `MT`） | 生存セルのたびに `BitCellSet::Union` を呼ぶ。しかも書き込み先の `areas[index]` は、スレッド間で隣接した `Rect` なので false sharing が起きる |
| D | `Game::ClearOutside` / `ClearRow` | 1 セルずつ `SetOnly(…, false)` で消している |
| E | `ThreadUtility::ForEach` | 毎世代 `std::jthread` を CPU コア数ぶん生成し、join している |
| F | `ThreadUtility::ForEach` | area が小さいときもスレッドに分割するので、オーバーヘッドが計算量を上回る |
| G | `BoardPainter::Paint` | 毎フレーム `CreateBitmapIndirect`、`CreateCompatibleDC`、`DeleteDC`、`DeleteObject` を実行している |
| H | `MainWindow::Next` → `SetTitle` | 毎世代 `SetWindowText` を呼び、非クライアント領域の再描画を引き起こしている |
| I | `Game::Randomize` | `random.Next() % 2` で 32 ビットの乱数を 1 セルに 1 回使い捨てている |
| J | ビルド設定 | Release に `/arch:AVX2` の指定がない |

## 3. 実施ステップ

効果の大きさと、他のステップとの依存関係で順番を決めた。各ステップは独立したコミットにする。

### Step 0: ベンチマークの用意

改善の効果を数値で確かめるため、ウィンドウを使わないベンチマークを用意する。

- `Shos.LifeGame.Test/` にベンチマーク用のソース（例: `Shos.LifeGame.Benchmark.cpp`）と、実行スクリプト（例: `run_benchmark.sh`）を追加する。
  - 計測ケース 1: 1000×1000 のランダムな盤面で、`Game::Next` を N 世代（例: 1000 世代）実行した時間
  - 計測ケース 2: 小さなパターン（グライダー、R-pentomino など）で、同じく N 世代実行した時間。`AREA` と `MT` のしきい値（Step 4）の効果を見るために使う
  - 計測ケース 3: 大きな既存パターン（`CellData` 内の大型 RLE など）
- 16 通りの組み合わせについて、`-O2` でビルドして計測する。回帰テストのスクリプトは `-O1 -D_DEBUG` なので、計測には使えない。
- 計測結果は、このファイルの「6. 計測結果」に追記していく。
- Windows での描画を含む FPS は、`Shos.LifeGame.cpp` の `LAP_TIMES` を有効にして計測する。

### Step 1: 近傍カウントの高速化（A）【`FAST`】

`FAST` が有効なときの `GetAliveNeighborCount` と、それを呼んでいる `NextPart` のループ構造を変える。

1. **内部と外周を分ける。** ボードの外周 1 セル分だけ境界チェック付きの経路で計算し、内部は境界チェックなしで処理する。
2. **3 行のスライディングウィンドウ。** 上・中・下の 3 行の先頭を指すポインタ（`USEBITS` のときはバイト列、無効のときは `bool` の行）を保持する。x を 1 つ進めるたびに、縦 3 セルの列和を 1 列ぶんだけ新しく計算し、残りの 2 列ぶんは使い回す。読み出し回数は 1 セルあたり 8 回から約 3 回に減る。
3. `USEBITS` 有効時は、3 行それぞれのビットをバイト単位で読み込み、シフトで取り出す。セルごとに index を計算し直さない。

- `GetAliveNeighborCount(const Point&)` は回帰テストなどから単体でも呼ばれるので、インターフェイスは残す。
- 非 `FAST` の経路には手を入れない。

### Step 2: 結果のバイト単位書き込み（B）【`USEBITS` && `FAST`】

`NextPart` で、次の世代の 8 セルぶんをレジスタ上で 1 バイトに組み立て、1 回の代入で `subBoard` に書き込む。

- 行の両端のバイトは計算範囲（area）の外側のビットを含むので、マスクして既存の値を残す。
- Step 1 のループと同時に書き換える。コミットは分ける。

### Step 3: `AREA` の更新処理とクリア処理の改善（C、D）【`AREA`】

1. **area の集計方法を変える。**
   - 行ごとに「最初の生存セルの x」「最後の生存セルの x」「生存セルの有無」を求め、行の終わりで 1 回だけ area を更新する。
   - `MT` 有効時は、スレッドのローカル変数に集計し、スレッドの終了時に 1 回だけ `areas[index]` へ書き戻す。これで false sharing がなくなる。
   - `BitCellSet::Union`（周囲 1 セルぶんを広げる処理とボード範囲へのクランプ）と同じ結果になることを、`_DEBUG` の `assert` と回帰テストの `AreaSurroundsPattern`、`AreaShrinks`、`AreaFollowsGlider` で確認する。
2. **`ClearOutside` をまとめて消す。**
   - 行全体を消すときは、`USEBITS` 有効時はその行のバイト範囲を `std::fill_n`（または `memset`）で、無効時は `bool` 行を `std::fill_n` で消す。
   - 行の一部を消すときは、両端のバイトにマスクをかけ、間のバイトをまとめて 0 にする。
   - `SetOnly` を 1 セルずつ呼ぶ実装は、非 `FAST` 経路の参照実装として残すかどうかを、実施時に判断する。

### Step 4: スレッドの常駐化と分割のしきい値（E、F）【`MT`】

1. **スレッドプールの導入。**
   - `Game` の生成時に「CPU コア数 − 1」本のワーカースレッドを起動し、`Game` の破棄時に停止する。メインスレッドも 1 区画を担当する。
   - 世代の開始と終了の同期には `std::barrier`（C++20）を使う。
   - `ThreadUtility::ForEach` のシグネチャ（`minimum`、`maximum`、`action(begin, end, threadIndex)`）はそのまま残し、中身だけを入れ替える。
   - `Game` はコピー禁止なので、所有関係は単純に保てる。ただし、ムーブや破棄時の join の順序には注意する。
2. **分割のしきい値。**
   - area の行数（またはセル数）がしきい値より少ない世代は、メインスレッドだけで計算する。
   - しきい値は Step 0 のベンチマークで決める。最初は 64 行程度で試す。

- 回帰テストは `-pthread` かつ ASan/UBSan（`--asan`）でも通ることを確認する。ThreadSanitizer（`-fsanitize=thread`）でも 1 回確認する。

### Step 5: 描画の改善（G、H）【Win32 側、オプションに依存しない】

1. **ビットマップの使い回し。**
   - 毎フレーム HBITMAP を作り直すのをやめ、`SetDIBitsToDevice` でボードのビット列を直接転送する。1bpp なので `BITMAPINFO` に白黒 2 色のパレットを持たせる。
   - DIB は bottom-up が既定なので、`biHeight` を負の値にして top-down にする。行は DWORD 境界に揃える必要があるので、`BitCellSet::GetRowByteNumber`（現在は WORD 境界）との違いに注意する。行バイト数を DWORD 境界に変える場合は、回帰テストの `BitmapRowIsWordAlignedPackedBits` の期待値も見直す。
   - 行バイト数を変えたくない場合は、`CreateBitmapIndirect` の代わりに、HBITMAP とメモリ DC を `BoardPainter` に保持して `SetBitmapBits` で更新する案にする。どちらにするかは計測して決める。
2. **タイトル更新の間引き。**
   - `SetTitle` を呼ぶのは、前回の更新から 100ms 以上経ったときだけにする。
   - FPS の計算は世代数と経過時間から求めているので、間引いても値は変わらない。

### Step 6: 小さな改善（I、J）

1. **`Randomize`**: `random.Next()` の 32 ビットを 32 セルぶんに使う。乱数列の使い方が変わるので、乱数の値に依存するテストがないか（`RandomBoardMatchesReference` は独自のシードを使っているか）を確認する。
2. **ビルド設定**: Release|x64 に `/arch:AVX2` を追加して計測する。効果がない、または配布先の CPU 要件が問題になる場合は取り下げる。

## 4. 検証方法

各ステップで次のすべてを満たすことを確認する。

1. `./Shos.LifeGame.Test/run_regression_tests.sh` が 16 通りすべてで成功する
2. `./Shos.LifeGame.Test/run_regression_tests.sh --asan` が成功する（Step 1〜4）
3. Visual Studio の Debug|x64 と Release|x64 のビルドが警告なしで通る
4. Debug ビルドで実行し、`Game::Test` の `assert` に引っかからない
5. Step 0 のベンチマークで、速くなっている（少なくとも遅くなっていない）

## 5. リスクと対策

| リスク | 対策 |
|--------|------|
| 内部と外周を分けたことによる境界の計算ミス | 回帰テストの `OutsideOfBoardIsDead`、`GliderVanishesAtCorner` と、ランダム盤面の参照実装との比較（`RandomBoardMatchesReference`）で検出する。必要なら、ボードの幅が 8 の倍数でないケースのテストを追加する |
| バイト単位の書き込みが area 外のビットを壊す | 幅が 8 の倍数でないボードや、area の端がバイトの途中にあるケースのテストを追加する |
| スレッドプールのデッドロックや終了時のハング | `std::barrier` の使い方を 1 か所に閉じ込め、TSan で確認する。`Game` を繰り返し生成・破棄するテストを追加する |
| 描画方式の変更で表示が上下反転したり、ずれたりする | 実機で目視確認する。`BitmapLeftmostPixelIsMostSignificantBit` などのビットマップ関連テストを維持する |
| 教材としての読みやすさの低下 | 各関数に、何をどう速くしたかのコメントを残す。README.md、README.jp.md の該当説明も更新する |

## 6. 計測結果

### 6.1 計測の条件

- 環境: WSL2 (Linux 6.18) 上の g++ 13.3、`-O2 -DNDEBUG`、8 スレッド
- ベンチマーク: `Shos.LifeGame.Test/run_benchmark.sh`（1000×1000 の盤面で `Game::Next` を繰り返す）。各ケースを 3 回実行し、最も速い値を採った。
  - Random: 半分のセルが生きているランダムな盤面
  - Glider: 中央にグライダー 1 つ
  - Gun_Gunstar、Max: `CellData` のパターン
- WSL では、Windows 側の負荷によって同じ条件でも値が 2 倍程度ぶれた。そのため、比べる 2 つの版を交互に実行し、それぞれの最小値を比べた。下の表の値には、±20% 程度の誤差があると考えてほしい。
- Windows (MSVC) のアプリのタイトルバーの FPS は測っていない。記事のベンチマークの数値は、Windows で測り直す必要がある。

### 6.2 Step 0（着手前）と Step 6（完了後）の比較

単位は 1 世代あたりのミリ秒。「0.000」は 0.0005 ミリ秒未満を表す。

| 組み合わせ | ケース | 着手前 | 完了後 | 倍率 |
|-----------|--------|-------:|-------:|-----:|
| FAST | Random | 16.754 | 4.951 | 3.4 倍 |
| FAST | Gun_Gunstar | 16.911 | 1.201 | 14.1 倍 |
| USEBITS FAST | Random | 25.423 | 8.013 | 3.2 倍 |
| USEBITS FAST | Gun_Gunstar | 18.971 | 3.571 | 5.3 倍 |
| FAST MT | Random | 7.849 | 1.631 | 4.8 倍 |
| USEBITS FAST MT | Random | 9.813 | 2.718 | 3.6 倍 |
| FAST AREA | Random | 20.292 | 4.014 | 5.1 倍 |
| FAST AREA | Gun_Gunstar | 0.156 | 0.017 | 9.2 倍 |
| USEBITS FAST AREA | Random | 23.941 | 7.813 | 3.1 倍 |
| MT AREA | Glider | 0.643 | 0.002 | 約 300 倍 |
| MT AREA | Max | 0.566 | 0.326 | 1.7 倍 |
| FAST MT AREA | Random | 9.138 | 1.423 | 6.4 倍 |
| FAST MT AREA | Gun_Gunstar | 0.560 | 0.021 | 26.7 倍 |
| FAST MT AREA | Max | 0.489 | 0.007 | 69.9 倍 |
| **USEBITS FAST MT AREA（既定）** | **Random** | **11.682** | **3.060** | **3.8 倍** |
| **USEBITS FAST MT AREA（既定）** | **Glider** | **0.475** | **0.000** | **1000 倍以上** |
| **USEBITS FAST MT AREA（既定）** | **Gun_Gunstar** | **0.598** | **0.058** | **10.3 倍** |
| **USEBITS FAST MT AREA（既定）** | **Max** | **0.528** | **0.016** | **33.0 倍** |

- `FAST` を使わない組み合わせ（NONE、USEBITS、MT、AREA など）は、計算の部分を変えていないので、Random では誤差の範囲（0.9〜1.3 倍）だった。ただし MT と AREA を両方使う組み合わせでは、小さな範囲をスレッドに分けなくなったので、Glider などが速くなった。
- 既定の組み合わせで、小さなパターンが大きく速くなったのは、Step 4 の効果である。これまでは毎世代のスレッドの生成（約 0.5 ミリ秒）が、計算そのものより長くかかっていた。

### 6.3 各ステップの効果

- **Step 1〜3（`FAST` の行単位の計算、バイト単位の書き込み、`AREA` のまとめた更新）**: `FAST` を使う組み合わせで 3〜16 倍。`bool` の盤面（`USEBITS` なし）のほうが、ビットの盤面より効果が大きかった。ビットの盤面では、1 セルを読むたびにシフトとマスクが必要なためである。
- **Step 4（スレッドの常駐化と分割のしきい値）**: しきい値を「1 スレッドあたりのセル数」で 4096 / 16384 / 65536 / 262144 と変えて測り、`FAST` では 65536 にした。これより小さいと小さなパターンでスレッドを起こす費用が残り、大きいと Random でスレッドが足りなくなった。`FAST` なしでは 1 セルの計算が重いので 1024 にした。
- **Step 5（描画）**: Linux では測れない。Windows で確認する必要がある。
- **Step 6**: `Randomize` は計測の対象外（盤面を作り直すときだけ呼ばれる）。`/arch:AVX2` の代わりに g++ の `-mavx2` で測ったところ、既定の組み合わせでは差がなかった（0.97〜1.04 倍）ので、追加しなかった。

### 6.4 計画から変えた点

- **Step 1〜3 は 1 つのコミットにまとめた。** 3 つとも `Game::NextRow` の同じループを書き換えるためである。
- **Step 4 では、`ThreadUtility` を `ThreadPool` に名前を変えた。** スレッドを持つクラスになり、`Game` のメンバーとして持つようになったためである。`ForEach` の引数の形（`minimum`、`maximum`、`action(begin, end, threadIndex)`）は残し、しきい値の引数を 1 つ追加した。記事（`Articles(Japanese)/`）のコード例は `ThreadUtility::ForEach` のままである。
- **Step 4 のしきい値は、行数ではなくセル数にした。** 計画では 64 行程度としていたが、16 行で試したところ、Gun_Gunstar などで 1 スレッドで計算するより 10 倍以上遅かった。範囲の幅によって 1 行の重さが変わるので、セル数で決めるようにした。
- **Step 5 は `SetBitmapBits` の案を採った。** `SetDIBitsToDevice` は行を DWORD 境界にそろえる必要があり、`BitCellSet` のビット配置と回帰テストの期待値を変えることになるためである。ビット配置は変えていない。
- **ベンチマークのグライダーの定義に誤りがあった**（9 文字のところを 8 文字にしていた）。直した後の値で比べている。

### 6.5 未確認の事項

- **Windows (MSVC) でのビルドと実行は確認していない。** この環境からは Windows のビルドツールを実行できなかった。特に Step 5（`ShosLifeGameBoardPainter.h`、`Shos.LifeGame.cpp`）は、Linux ではコンパイルもできない。Visual Studio で Debug と Release をビルドし、表示とタイトルバーの FPS を確認する必要がある。
- 記事と README のベンチマークの数値（16 → 244 FPS など）は、着手前の Windows での値のままである。
