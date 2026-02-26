# C++/Win32で学ぶ高速ライフゲーム実装

<img src="https://github.com/Fujiwo/Shos.LifeGame/blob/main/Articles(Japanese)/Images/lifegame01.png?raw=true" alt="Life Game"></img>

- [実行のイメージ | YouTube](https://youtu.be/ny-VKZEAVkg)

## はじめに
[ライフゲーム](https://ja.wikipedia.org/wiki/%E3%83%A9%E3%82%A4%E3%83%95%E3%82%B2%E3%83%BC%E3%83%A0)は、ルール自体は驚くほど簡単なのに、実装に入った瞬間に「なぜこんなに遅いのか」という現実に直面する題材である。  
同じ盤面、同じルールでも、データの持ち方やループの回し方、描画への渡し方が変わるだけで体感速度は大きく変わる。  
本記事は筆者がGitHubにあげた[Shos.LifeGame](https://github.com/Fujiwo/Shos.LifeGame) を題材に、実装を読む順番そのものを道しるべにしながら、どこで時間を使い、どこで短縮しているのかを追う。結論だけ先に言えば、この実装の核心は `USEBOOL` / `FAST` / `MT` / `AREA` / `BoardPainter` という5つの高速化要素が、別々のボトルネックに効くように配置されている点にある。

- この記事で持ち帰れるポイント:
  1. 様々な最適化方法とその効果
  2. 実行速度とそれ以外のトレードオフ
- 対象読者:
  - C++初級後半〜中級(`std::thread` と前処理マクロの基本を理解している読者)
- 実際のソースコード:
  - [Shos.LifeGame | GitHub](https://github.com/Fujiwo/Shos.LifeGame)

まずはライフゲームの実行画面のイメージをつかんでほしい。

#### 図1: 世代更新と描画を繰り返しながら、パターンが時間発展する様子を示す
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/refs/heads/main/Articles(Japanese)/Images/ShosLifeGame_Figure01_Overview.svg"  alt="図1 Shos.LifeGame 実行画面イメージ"></img>
※ 実際の実行画面とは異なる

---

## 1. ライフゲームとは何か

### 1.1 概要

ライフゲーム(Conway's Game of Life)は、1970年にイギリスの数学者ジョン・ホートン・コンウェイが考案した、生命の誕生や淘汰を簡易的なモデルで再現したシミュレーションゲームである。

- 概要
  - ゼロプレイヤーゲーム: プレイヤーが操作するのではなく、最初にセルの配置(初期状態)を決めた後は、特定のルールに従って自動的に変化が進む様子を観察する
  - セルオートマトン: 格子状のセル(細胞)が「生」か「死」の状態を持ち、周囲の状況に応じて次の世代の状態が決定される

- 基本ルール:
あるセルの次の世代の状態は、その周囲8マスの「生きているセル」の数によって決まる

  - 誕生: 死んでいるセルの周囲に、生きているセルがちょうど 3個 あると、次世代で「誕生」する
  - 生存: 生きているセルの周囲に、生きているセルが 2個または3個 あると、次世代でも「生存」する
  - 過疎: 生きているセルの周囲に、生きているセルが 1個以下しかないと、寂しさ(過疎)で「死滅」する
  - 過密: 生きているセルの周囲に、生きているセルが 4個以上 あると、混みすぎ(過密)で「死滅」する 

- パターン
単純なルールだが、初期配置によって非常に複雑で興味深い動きを見せる
  - 固定物体: 形が全く変わらず静止し続けるもの
  - 振動子: 一定の周期で形を変化させながらその場に留まるもの(例:ブリンカー)
  - 移動物体: パターンを繰り返しながら画面上を移動していくもの(例:グライダー)

プログラミングの課題としても非常に人気がある。

実装側で行うことも本質的には単純で、「毎世代この判定を盤面全体に適用し、次世代の状態へ置き換える」というのを繰り返すだけである。

**図2. ライフゲームの更新ルール(3x3近傍)**  
中心セルの次状態は8近傍の生セル数で決まり、誕生と生存の条件が分かれる。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure02_Rules.svg" alt="図2 LifeGame ルール図"></img>

### 1.2 なぜ実装で差が出るのか
性能差が生まれる理由は、同じ計算をしていても「どんな形でメモリに置き」「どんな順で読み書きし」「どこまでを処理対象にするか」が実装ごとに違うからである。  
`Shos.LifeGame` では、bit表現とbool表現の切替、汎用反復と直接ループの切替、並列化、活動領域限定、さらに描画転送の最適化までを段階的に分離している。

この分離があるおかげで、「速いか遅いか」だけでなく「何が効いたのか」を追跡できる。次章では、その追跡を迷わず進めるために、まず実装全体の地図を作る。

---

## 2. Shos.LifeGameの全体像
`Shos.LifeGame` の特徴は、主要ロジックが `ShosLifeGame.h` にまとまっているため、全体像を掴みやすいことである。
役割の中心には `Game` があり、世代更新の進行と盤面管理を担当する。盤面そのものは `Board` と `BitCellSet` が持ち、入力は `PatternSet` が `.lif` / `.rle` から読み込み、最後の表示を `BoardPainter` が担当する。
実行時には `MainWindow::Next` が1ステップの起点となり、`game.Next()` で盤面を進めた直後に再描画して、世代数とFPSをタイトルへ反映する。
この「入力→更新→描画」の流れを先に頭へ入れておくと、後続の章で個別最適化を見たときに、どの層に作用している変更なのかを即座に判断できる。

図3はこの責務分担を1枚で示した地図として使ってほしい。

#### 図3: 主要クラスの関係
`MainWindow` が更新を駆動し、`Game` が盤面状態を管理、`BoardPainter` が描画を担当する。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/refs/heads/main/Articles(Japanese)/Images/ShosLifeGame_Figure03_ClassDiagram.svg" alt="図3 クラス関係図"></img>

---

## 3. パターンファイル形式とCellDataの読み解き
### 3.1 [`.lif`(Life 1.05)形式](https://conwaylife.com/wiki/Plaintext)
`.lif` は、見た目に近い形でセル配置を行テキストとして持つ形式である。`Shos.LifeGame` の実装では、まず `#` で始まるコメント行を取り除き、次に行幅を揃えてから、生セル `*` と死セル `.` の内部表現へ連結する。  
この手順の利点は、ファイルを目で追いやすい点にある。形式が素直なので、パターンの意図を理解しながらデータ化できるのが `.lif` の強みである。

例. Gosper glider gun
```
........................O
......................O.O
............OO......OO............OO
...........O...O....OO............OO
OO........O.....O...OO
OO........O...O.OO....O.O
..........O.....O.......O
...........O...O
............OO
```

### 3.2 [`.rle`(Run Length Encoding)形式](https://conwaylife.com/wiki/Run_Length_Encoded)
`.rle` は、同じ状態が続く区間を圧縮して書く形式である。実装側では最初にヘッダ(`x=...`, `y=...`)からサイズを取り出し、その後に本文の `b` / `o` / `$` / `!` を展開して盤面データへ戻す。  
人間にとってはやや記号的だが、大きなパターンをコンパクトに扱えるため、配布や共有の実用性が高い形式である。

例. Gosper glider gun
```
x = 36, y = 9, rule = B3/S23
24bo11b$22bobo11b$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o14b$2o8b
o3bob2o4bobo11b$10bo5bo7bo11b$11bo3bo20b$12b2o!
```

#### 図4: `.lif` と `.rle` の表現対応**
同じセル形状でも、`.lif` は行テキスト、`.rle` は連長圧縮で記述される。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure04_LifRleMapping.svg" alt="図4 LIFとRLEの対応"></img>

### 3.3 `CellData` 同梱パターンの使い分け
`CellData` の価値は、単なるサンプル集ではなく、読み手が負荷特性の違いを追体験できる教材になっている点にある。たとえば `Spaceship_Glider.lif` を見ると、局所的な変化が盤面を移動する様子が分かる。そこから `Gun_GosperGliderGun.lif` へ進むと、継続的に構造が放出されるため、更新対象が時間とともに広がっていく。さらに `OTCA_metapixel.rle` のような大規模パターンでは、更新・描画ともに重くなる条件が明確になり、どの最適化が効きやすいかを考える足場ができる。  
以下の表は、その学習順序を崩さずにパターンを選ぶためのガイドとして使える。
あわせて図5を見ると、カテゴリ同士の位置関係と用途の違いを直感的につかめる。

| カテゴリ | 代表例 | 使いどころ |
|---|---|---|
| [Gun](https://conwaylife.com/wiki/Gosper_glider_gun) | [`Gun_GosperGliderGun.lif`](https://conwaylife.com/patterns/gosperglidergun.cells) | 長期的に発生し続ける挙動の例 |
| Oscillator | `Oscillator_Pulsar.lif` | 周期運動の理解 |
| Spaceship | `Spaceship_Glider.lif` | 移動パターンと領域追跡の説明 |
| Methuselah | `Methuselah_Acorn.lif` | 初期単純・長期複雑化の例 |
| Puffer / Engine | `Puffer_Puffer1.lif` | 広範囲に変化する高負荷ケース |
| Breeder / Meta | `Breeder1.lif`, `OTCA_metapixel.rle` | 大規模化・自己増殖系の話題 |

#### 図5: `CellData` パターン分類マップ
Gun/Oscillator/Spaceship などのカテゴリで、用途に応じたパターン選定を行える。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure05_CellDataCategories.svg" alt="図5 CellDataカテゴリマップ"></img>


---

## 4. データ構造と更新処理の基礎
### 4.1 `Board` と `BitCellSet`
既定構成(`USEBOOL` 無効)では、`Board` は `BitCellSet` を継承して1bit/セルで盤面を保持する。これによりメモリ効率が高く、後段の描画処理へビット列を渡しやすくなる。  
一方で `USEBOOL` を有効化すると、`bool**` を使う別実装へ切り替わる。こちらは理解しやすさに利点があるが、表現密度や変換コストの観点ではトレードオフが生まれる。

### 4.2 `Game::Next` と `Game::NextPart`
世代更新の入口は `Game::Next` で、ここから実際の更新処理が `NextPart` へ流れる。`MT` が有効ならY方向に分割して並列実行され、`AREA` が有効なら活動領域に処理対象を絞り込み、`FAST` が有効なら直接ループ中心で反復コストを抑える。  
つまりこの層では、「何を計算するか」は同じでも「どう計算を配るか」を切り替えている、という理解が重要である。

### 4.3 `BoardPainter` による描画
描画段では、`Board::GetBits()` で得た盤面ビット列を `CreateBitmapIndirect` へ渡し、最終的に `BitBlt` で画面へ転送する。  
この構成のポイントは、更新ロジックと描画ロジックを分離しながら、データ表現をできるだけ中間変換なしで橋渡ししていることにある。更新で作った情報がそのまま描画へ流れるため、処理全体を一本のパイプラインとして捉えやすくなる。次章では、このパイプラインのどこを5要素で最適化しているかを個別に分解する。
図6で更新パイプライン全体を確認し、図7でそのうち描画経路だけを拡大して読むと、処理の層を分離して理解しやすい。

#### 図6: 更新パイプライン
`MainWindow::Next` から `Game::Next` を経由して、最終的に `BoardPainter::Paint` で表示される。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure06_UpdatePipeline.svg" alt="図6 更新パイプライン"</img>

#### 図7: 盤面ビット列からBitmap描画への変換
`Board::GetBits()` の出力をBitmapに渡し、`BitBlt` で画面転送する流れを示す。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure07_BitsToBitmap.svg" alt="図7 ビット列から描画まで"></img>

---

## 5. 高速化テクニック5要素の実装読解
第4章までで見た更新パイプラインに、どの要素がどこで効くのかを順に重ねていく。ここで重要なのは、「最適化は万能な加速ボタンではなく、効く場所が異なる部品である」という見方である。
先に全体像を掴むために、5要素の比較を1表で置いておく。

| 要素 | 内容 | 主に効く場所 | 効きやすい条件 | 主な副作用 |
|---|---|---|---|---|
| USEBOOL(FALSE推奨) | セルをビットでなく bool で保持 | セルデータ保持/更新/描画 | ビット表現を維持したい場合 | TRUE時はメモリ増・変換負荷増 |
| FAST | 直にループ | 繰り返し処理 | 呼び出しオーバーヘッドを削りたい場合 | コード重複が増える |
| MT | マルチスレッド | 更新計算 | 大盤面・多コア環境 | 同期コスト・環境依存 |
| AREA | 走査範囲の最適化 | 走査範囲 | 疎なパターン | 領域管理が複雑化 |

### 5.1 USEBOOL ("FALSE" のとき: ビット演算とビットパターン転送による描画)
```cpp
//#define USEBOOL // Boolean enabled
#if defined(USEBOOL)
class Board final { bool** cells; };
#else
class Board final : public BitCellSet {};
#endif
```
`USEBOOL` は盤面の保持形式を切り替えるための分岐で、ビット演算を減らして実装理解をしやすくする余地がある。その一方で、表現密度が下がるためメモリ使用量が増えやすく、描画側で `GetBits()` を使うときの変換コストが目立つ場面も出てくる。
速度面では、`USEBOOL` を FALSE(未定義)にして、 `BitCellSet` を使うほうが有利になる。盤面の更新がビット演算となり、また、描画がビット列の転送(`GetBits()` 経由) で行えるためである。もちろん、メモリーの使用量も大幅に減る。

### 5.2 FAST (直接ループ)
```cpp
#if defined(FAST)
for (Point p = start; p.y < endY; p.y++)
    for (p.x = startX; p.x < endX; p.x++)
        Set(p, pattern[idx++]);
#else
Utility::ForEach(Rect(start, size), [&](const Point& p) { Set(p, pattern[idx++]); });
#endif
```
`FAST` は、汎用反復よりも直接ループを優先することで、呼び出しオーバーヘッドを抑える方針である。実行速度には効きやすい反面、コードの重複が増えやすく、抽象化レベルは下がるため保守性とのバランス判断が必要になる。

### 5.3 MT (マルチスレッド)
```cpp
ThreadUtility::ForEach(area.leftTop.y, areaRightBottom.y,
    [=](Integer minY, Integer maxY, unsigned int index) {
        NextPart(Point(area.leftTop.x, minY), Point(areaRightBottom.x, maxY), areas[index]);
    });
```
`MT` は更新領域をY方向に分割し、`NextPart` を並列実行してスループットを上げる要素である。とくに盤面が大きい条件では有効だが、同期やスレッド管理のコストがあるため、小さな盤面では期待した伸びが出ないこともある。

### 5.4 AREA (走査領域の限定)
```cpp
const auto area = mainBoard->GetArea();
NextPart(area.leftTop, area.RightBottom());
```
`AREA` は活動領域だけを更新対象にすることで、死セルばかりの領域を計算から外す考え方である。疎なパターンでは計算量を大きく減らせるが、その効果を支えるために領域管理ロジックは複雑になり、検証時の観点も増える。

### 5.5 描画高速化(`ShosLifeGameBoardPainter.h`)
`USEBOOL` が無効の既定構成では、`Board` は `BitCellSet` 派生として `UnitInteger* cells` を持ち、更新で書き換えたビット列を `GetBits()` で直接取り出せる。`BoardPainter` はそのポインタを `BITMAP::bmBits` に渡して `CreateBitmapIndirect` を作成し、最後に `BitBlt` で画面へ転送する。  
この流れの本質は、更新結果を別形式へ組み替えずに描画へ橋渡ししている点にある。ピクセル単位の手作業ループを避けられるため、描画コストを抑えやすくなる。ただし実装は1bpp/GDI前提なので、可搬性よりも速度寄りの選択だと捉えるのが適切である。

```cpp
bitmap.bmBits = board.GetBits();
return ::CreateBitmapIndirect(&bitmap);
```

```cpp
::BitBlt(deviceContextHandle, position.x, position.y, size.cx, size.cy,
         memoryDeviceContextHandle, 0, 0, SRCCOPY);
```


#### 図8: 最適化テクニックの適用レイヤー
`USEBOOL`/`FAST`/`MT`/`AREA`/`BoardPainter` が、それぞれデータ表現・ループ・並列・処理範囲・描画転送に作用する。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure08_OptimizationMap.svg" alt="最適化マップ"></img>

#### 図9: 処理方式の比較イメージ
全面走査と領域限定、単一スレッドと分割処理の違いを視覚的に比較する。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure09_StrategyComparison.svg" alt="処理方式の比較"></img>

---

## 6. 比較検証の進め方
### 6.1 比較条件
比較では、1回につき切り替える要素を1つに限定し、盤面サイズ・初期パターン・世代数を固定する。また Debug と Release を混在させると解釈が曖昧になるため、評価は同一ビルド条件で揃えるのが前提である。
本章のLevel表は、前Levelから要素を1つずつ追加する累積比較として示している。

### 6.2 指標
主要指標は、世代/秒(FPS相当)と総実行時間である。可能であればメモリ使用量も併記すると、速度だけでなく副作用も含めた比較になる。

### 6.3 結果の読み解き
結果を読むときは、条件依存を先に意識する。小盤面では `MT` の利得が出にくく、疎なパターンでは `AREA` が効きやすい一方、どの要素も常に最速を保証するわけではない。

### 6.4 最新ベンチ結果を「高速化の成果」として示す
`Articles(Japanese)/ShosLifeGame_BenchmarkData.csv` には、Optimization Level 0〜4の実測値が記録されている。

| Level | USEBOOL | FAST | MT | AREA | 実行時間 | FPS | FPS倍率 |
|---:|:---:|:---:|:---:|:---:|---:|---:|---:|
| 0 | TRUE | FALSE | FALSE | FALSE | 63.8s | 16 | 1.00 |
| 1 | FALSE | FALSE | FALSE | FALSE | 60.2s | 17 | 1.06 |
| 2 | FALSE | TRUE | FALSE | FALSE | 24.2s | 41 | 2.56 |
| 3 | FALSE | TRUE | TRUE | FALSE | 11.7s | 86 | 5.38 |
| 4 | FALSE | TRUE | TRUE | TRUE | 4.1s | 244 | 15.25 |

ただし、これは、あくまで共有された実行環境での結果であり、CPU、ビルド設定、実行条件の違いで値は変わる。

#### 図10: Shos.LifeGame ベンチ結果(Optimization Level 0〜4)
測定条件はPattern `Gun_Gunstar`、盤面サイズ `1000x1000 (1000,000セル)`、反復 `1000` 回。ビルド設定は、Release。CPU・実行環境により結果は変動する。
<img src="https://raw.githubusercontent.com/Fujiwo/Shos.LifeGame/0b8c8159d4726cb7bac51249b6c6112e0778b79c/Articles(Japanese)/Images/ShosLifeGame_Figure10_Benchmark.svg" alt="図10 Shos.LifeGame ベンチ結果"></img>


---

## 7. まとめ
`Shos.LifeGame` では、ビット表現、ループ最適化、並列化、活動領域追跡、描画転送最適化を、比較している。

`Shos.LifeGame/ShosLifeGame.h` の頭の方にある下記の各 #define をコメントアウトしてビルドしなおすことで、切り替えることができる。

```cpp
//#define USEBOOL // Boolean enabled
#define FAST    // Fast loops enabled
#define MT      // Multi-threading enabled
#define AREA    // Area enabled
```

本題材は、単なるLifeGame実装にとどまらず、データ構造・並列処理・計測といった実践的な最適化観点を横断して学ぶ入口になる。
また、実行速度を上げることでコードの複雑化が起こる様子も見ることができる。
IDE で、`USEBOOL` / `FAST` / `MT` / `AREA` のON/OFFを行うことで、そうしたトレードオフ検証を行うことができるだろう。

---
