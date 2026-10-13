# Shos.LifeGame リポジトリ全体レビュー

- レビュー日: 2026-10-13
- 対象: `main` ブランチ HEAD (`d03fdb0`)
- 対象範囲: ソースコード (`Shos.LifeGame/`, `Shos.LifeGame.Test/`)、プロジェクト設定 (`*.vcxproj`)、パターンデータ (`CellData/`)、ドキュメント (`README.md`, `Articles(Japanese)/`)、リポジトリ運用
- 方法: 静的レビュー (コードの読解)。Linux 環境のため MSVC でのビルドや実行による確認はしていない。「要実機確認」と書いた項目は Windows 上での確認を推奨する。

---

## 1. 総評

C++20 と Win32 API だけで、ビット表現 (`USEBITS`)・高速ループ (`FAST`)・マルチスレッド (`MT`)・処理範囲の限定 (`AREA`) の 4 要素を `#define` で切り替えて比べられる構成になっている。教材としての狙いがはっきりしていて、記事 (Articles) とも対応が取れており、全体の見通しは良い。

一方で、次のような問題がある。

- **メモリ管理の不具合** (`delete` と `delete[]` の取り違え、メモリリーク)
- **パターン読み込みの仕様漏れ** (RLE の `n$`、Life 1.05 の `!` 行と `#P` ブロック)
- **描画のビット順** (1bpp ビットマップの MSB/LSB)
- **ビット配列のサイズ計算の誤り** (メモリを 8 倍確保している)
- **現在の既定設定 (`USEBITS` 無効) と、記事・コミットメッセージの記述の食い違い**
- **ローカル `.git` のオブジェクト破損** (45 個)

`#define` の組み合わせは 16 通りあるが、自動ビルドも正しさを確かめるテストもないため、使われない組み合わせが壊れていても気付けない。

---

## 2. 重大度別の指摘一覧

| # | 重大度 | 分類 | 概要 | 場所 |
|---|---|---|---|---|
| 1 | 重大 | リポジトリ | ローカル `.git` のオブジェクトが 45 個破損している | `.git/objects` |
| 2 | 高 | メモリ | `new` で確保したものを `delete[]` で解放している (未定義動作) | `ShosLifeGame.h:680` |
| 3 | 高 | メモリ | `bool**` の各行を解放していない (リーク) | `ShosLifeGame.h:681`, `:762-767` |
| 4 | 高 | 正しさ | RLE の `n$` (複数行の改行) を 1 行として扱う | `ShosLifeGame.h:372-375` |
| 5 | 高 | 正しさ | Life 1.05 の `!` コメント行がパターンの行として読み込まれる | `ShosLifeGame.h:299` |
| 6 | 高 | 描画 | 1bpp ビットマップのビット順が逆で、8 ピクセル単位で左右反転して表示されるおそれ (要実機確認) | `ShosLifeGame.h:620-623`, `ShosLifeGameBoardPainter.h` |
| 7 | 中 | 性能/メモリ | `unitNumberX` を `sizeof` で割っているため、必要量の 8 倍を確保している | `ShosLifeGame.h:603-610` |
| 8 | 中 | 正しさ/性能 | `bool**` 版 `Board::Clear` が `std::min` を使っており、`area` が左上隅になる | `ShosLifeGame.h:777` |
| 9 | 中 | 性能 | `bool**` 版 `GetBits()` が毎フレーム `BitCellSet` を作り直し、全セルで `Union` を呼ぶ | `ShosLifeGame.h:654-669` |
| 10 | 中 | 設計 | `AREA` の範囲は広がるだけで縮まない | `ShosLifeGame.h:1000-1004`, `:551-576` |
| 11 | 中 | ビルド | Win32 構成に `stdcpp20` の指定がない | `Shos.LifeGame.vcxproj:73-100` |
| 12 | 中 | ビルド | `robocopy` の終了コード 1 (コピー成功) がビルドエラー扱いになるおそれ | `Shos.LifeGame.vcxproj:119`, `:148` |
| 13 | 中 | 整合性 | 既定の `USEBITS` が無効になっており、記事・コミットメッセージと食い違う | `ShosLifeGame.h:3` |
| 14 | 中 | ドキュメント | README の `.LIF/.LIFE` 対応という記述と実装 (`.lif` のみ、大文字小文字を区別) が食い違う | `ShosLifeGame.h:276`, `ShosHelper.h:85` |
| 15 | 低 | Win32 | `TIMER` / `USERMESSAGE` を有効にした経路の不具合 (現在は無効) | `ShosWin32.h`, `Shos.LifeGame.cpp:65` |
| 16 | 低 | C++ | `this == nullptr` の比較は未定義動作 | `ShosWin32.h:242` |
| 17 | 低 | C++ | Rule of Three/Five 違反 (所有している生ポインタ) | `BitCellSet`, `Board`, `Game` |
| 18 | 低 | テスト | テストプロジェクトがベンチマークのみで、正しさを検証していない | `Shos.LifeGame.Test.cpp` |
| 19 | 低 | その他 | ヘッダーの include 漏れ、`tstring` のマクロ定義など | `ShosHelper.h` ほか |
| 20 | 低 | ドキュメント | 記事中の文の切れ・見出しの結合、README の説明が実装とずれている | `Articles(Japanese)/ShosLifeGame_Article.md` ほか |

---

## 3. 詳細

### 3.1 [重大] ローカル `.git` のオブジェクト破損

`git fsck` を実行すると、`inflate: data stream error (unknown compression method)` が **45 個** 出る。`git show 77bb266` なども失敗する。

- 作業ツリーと HEAD は読めているが、履歴の参照や `gc`、push が失敗するおそれがある。
- リポジトリが Dropbox 配下 (`/mnt/c/dropbox/...`) にあるため、同期ソフトが `.git` の書き込み途中のファイルを同期したことが原因だと考えられる。

**推奨**
1. 未 push のコミットがないことを確認する (`git log origin/main..main`)。
2. GitHub から別の場所へ clone し直し、作業ツリーの差分 (現在は `LICENSE.txt` のみ変更あり) を移す。
3. 今後は `.git` を Dropbox の同期対象から外す (リポジトリを同期フォルダーの外に置く、または同期から除外する)。

あわせて、リポジトリ直下に `.bashrc`, `.profile`, `.gitconfig`, `.mcp.json`, `.idea`, `.claude/` などの未追跡ファイルがある。誤ってコミットしないよう、`.gitignore` に追加するか、置き場所を見直すことを勧める。

### 3.2 [高] `Board` のデストラクタでの `delete[]` の誤用とメモリリーク (`USEBITS` 無効時。現在の既定)

```cpp
// ShosLifeGame.h:657
bitCellSet = new BitCellSet(size);   // 単一オブジェクトの new
...
// ShosLifeGame.h:678-682
~Board()
{
    delete[] bitCellSet;   // ← new[] で確保していないので未定義動作
    delete[] cells     ;   // ← 行ポインタの配列だけを解放している
}
```

- `bitCellSet` は `new` で確保しているので、`delete bitCellSet;` で解放する必要がある。
- `cells[y] = new bool[size.cx]` (`:766`) で確保した各行を解放していない。1000×1000 の盤面なら 1 枚あたり約 1 MB がリークする。

**推奨**: `std::vector<bool>` か `std::vector<std::uint8_t>` (1 次元で `y * cx + x`)、または `std::unique_ptr<BitCellSet>` を使い、手動の `delete` をなくす。1 次元配列にすると、キャッシュ効率も上がる。

### 3.3 [高] RLE パーサーが `n$` を処理していない

```cpp
// ShosLifeGame.h:372-375
case '$':
    AppendPartPattern(partPattern, width, pattern);
    partPattern.clear();
    break;              // count (例: "3$" の 3) を無視している
```

RLE の `n$` は「n 行分の改行」を表す (空行が n−1 行入る)。現在の実装では 1 行しか進まないので、空行を含むパターンが縦方向に詰まって壊れる。

- 同梱ファイルのうち `CellData/OTCA_metapixel.rle` は `117b2$`, `1933b3$` などを含むので、正しく読み込めていない。

**推奨**: `$` のときは `count == 0 ? 1 : count` 回だけ行を確定し、2 行目以降は `width` 個の `.` で埋める。

その他の細かい点:
- 末尾に `!` がない RLE では、最後の行 (`partPattern`) が捨てられる (`:383`)。
- `GetRleCount` のループ条件が `index < length` になっているが、実際に参照しているのは `index + number` である (`:400`)。現状は `std::string` の終端 `'\0'` のおかげで止まるが、意図が読み取りにくい。
- `std::tolower` と `std::isdigit` に負の `char` を渡すと未定義動作になる。`static_cast<unsigned char>` を通すこと。
- ヘッダーの `rule = B3/S23` 以外のルールを無視している。B3/S23 以外のファイルを読み込まない、または警告を出すことを検討する。

### 3.4 [高] Life 1.05 の `!` 行と `#P` ブロックを扱っていない

```cpp
// ShosLifeGame.h:299
[](const std::string& text) { return !text.starts_with('#'); }
```

- `CellData/13_engine_Cordership.lif` は 2〜4 行目が `!Name: ...` のような `!` で始まるコメント行 (Plaintext 形式の流儀) になっている。これらの行がパターンの行として扱われるので、次の問題が起きる。
  - 上に 3 行の余白ができる。
  - 行の長さの最大値 (`width`) がコメントの長さで決まり、パターンが横にずれる。
- Life 1.05 の `#P x y` (ブロックの配置座標) を解釈していない。現在の同梱ファイルに複数ブロックのものはないが、外部から取り込んだファイルを正しく読めない。
- 空行も除外していない (RLE 側では除外している)。
- 記事 3.1 節のリンク先は Plaintext (`.cells`) 形式の説明で、Life 1.05 の仕様ではない。

**推奨**: `!` 行と空行も除外する。`#P` に対応するか、未対応であることを README に明記する。

### 3.5 [高] 1bpp ビットマップのビット順 (要実機確認)

```cpp
// ShosLifeGame.h:620-623
const auto bit = Byte(point.x % bitNumber);   // x=0 → bit0 (LSB)
...
cells[index] |= 1 << bit;
```

Windows のモノクロ DDB (`CreateBitmapIndirect` で `bmBitsPixel = 1`) では、**各バイトの最上位ビット (MSB) が最も左のピクセル** になる。現在の実装は x=0 を LSB に割り当てているので、画面上では 8 ピクセルごとに左右が反転して表示されるはずである。

ライフゲームは盤面全体がランダムだと見た目では気付きにくいが、グライダーなどの小さなパターンを表示すると形が崩れて見えるはずである。

**推奨**: `bit = 7 - (x % 8)` (`0x80 >> (x % 8)`) に変える。シミュレーション側は `Get`/`Set` を経由しているので影響を受けない。

### 3.6 [中] `BitCellSet` のサイズ計算 (8 倍の確保)

```cpp
// ShosLifeGame.h:603-610
unitNumberX = size.cx / sizeof(UnitInteger);   // sizeof(Byte) == 1 なので cx バイト
```

1 行に必要なのは `cx / 8` バイトだが、`cx` バイト確保している。`ToIndex` (`:621`) は `x / 8` で添字を計算し、`BoardPainter` も `bmWidthBytes = cx / sizeof(Byte)` (= cx) を使っているので、**行の幅 (stride) としては整合が取れていて動いている**。ただし次の無駄がある。

- 盤面 1 枚あたり 1 MB (本来は 125 KB)。
- `CreateBitmapIndirect` が毎フレーム 1 MB をコピーする。
- キャッシュ効率が悪い。

コメント `size.cx must be a multiple of 8` の意図 (8 で割る) と実装が一致していない。

**推奨**: `unitNumberX = (cx + 7) / 8` に変える。`bmWidthBytes` は WORD 境界 (偶数) に揃える必要があるので、`((cx + 15) / 16) * 2` とし、`unitNumberX` も同じ値にそろえる。

### 3.7 [中] `bool**` 版の `Board::Clear` の `area` 初期値

```cpp
// ShosLifeGame.h:777
area = Rect(Point(std::min(0, size.cx / 2 - 1), std::min(0, size.cy / 2 - 1)), ...);
```

`std::max` とすべきところが `std::min` になっているので、常に `(0,0)` になる。`BitCellSet::GetDefaultArea` (`:458`) と結果が違う。このため、パターンを読み込んだあとの `area` は「左上隅から中央のパターンまで」の範囲に広がり、`AREA` による範囲限定の効果が大きく下がる。

**推奨**: `area = BitCellSet::GetDefaultArea(GetRect());` を使い、計算を 1 か所にまとめる。

### 3.8 [中] `bool**` 版の描画変換のコスト

`Board::GetBits()` (`:654-669`) は描画のたびに次の処理をしている。

1. `BitCellSet` を `delete` して `new` し直す (1 MB の確保とゼロクリア)。
2. 全セルについて `BitCellSet::Set` を呼ぶ。`AREA` が有効なので、生きているセルごとに `Union` を計算し、Debug ビルドでは `assert` も評価する。

**推奨**: `BitCellSet` はメンバーとして 1 度だけ確保して使い回し、`Clear()` してから `SetOnly` で書き込む。ベンチマークの「Level 0 (USEBITS 無効)」の数値には、この変換コストが含まれている点に注意すること。

### 3.9 [中] `AREA` の範囲が縮まない

`Game::ResetAreas()` (`:1000-1004`) は各スレッドの範囲を **現在の `mainBoard` の範囲** で初期化してから `Union` で広げている。そのため、範囲は世代を重ねるたびに広がるだけで、縮むことはない (`MT` 無効の経路でも `subBoard` の過去の範囲に `Union` を重ねるので同じ)。

- ランダムな盤面 (起動時や右クリック時) では、最初の世代で範囲が盤面全体になり、それ以降は `AREA` の効果がなくなる。
- グライダーが飛んでいくパターンでは、範囲が軌跡全体に広がり続ける。

この性質のおかげで「範囲の外にある `subBoard` のセルは必ず死んでいる」という前提が保たれているので、正しさの面では問題ない。ただし記事のベンチマーク (Gunstar で 244 FPS) は「範囲が小さいまま保たれるパターン」での結果であり、一般的な速度向上を示すものではない。記事にこの点を注記することを勧める。

**改善案**: 新しい範囲は「生きているセルの外接矩形 + 1 マス」だけから計算し、範囲が縮んだときは前回の範囲との差分の領域を `subBoard` 側でクリアする。

### 3.10 [中] プロジェクト設定

- **Win32 (x86) 構成に `<LanguageStandard>stdcpp20</LanguageStandard>` がない** (`Shos.LifeGame.vcxproj:73-100`、Test 側も同じ)。`std::string::starts_with` は C++20 の機能なので、既定の C++14 ではビルドできない。Win32 構成を使わないなら、構成ごと削除するのがよい。
- Win32 構成は Level3、x64 構成は Level4 と、警告レベルがそろっていない。`/W4` に統一し、可能なら `/WX` も付けることを勧める。
- **PostBuild の `robocopy ... /MIR`**: robocopy はファイルをコピーすると終了コード 1 を返す。MSBuild は 0 以外を失敗として扱うので、`error MSB3073` になりやすい。
  例: `robocopy "$(ProjectDir)CellData" "$(OutDir)CellData" /MIR & if %ERRORLEVEL% LSS 8 exit /b 0`
- `PatternSet` が読み込むのは作業ディレクトリーからの相対パス `.\CellData` (`ShosLifeGame.h:257`) である。exe をエクスプローラーなどから起動すると、作業ディレクトリーが変わってパターンが 0 件になる。exe のパス (`GetModuleFileName`) を基準にすることを勧める。
- テストプロジェクトには CellData のコピー処理がない。ただし `Game` のコンストラクターで `PatternSet` を読むだけなので、実害はない。

### 3.11 [中] 既定の設定と記事・コミット履歴の食い違い

- `ShosLifeGame.h:3` は `//#define USEBITS` (無効) になっている。
- 一方、コミット `77bb266` のメッセージは「USEBITS のコメントを有効化し」で、記事の 5.1 節と 302 行目のコード例は `#define USEBITS` (有効) を前提にしている。

意図した既定値がどちらなのかを確認し、コード・README・記事をそろえること。なお、`USEBITS` 無効の経路には 3.2 / 3.7 / 3.8 の問題があるので、既定は有効にすることを勧める。

### 3.12 [中] README のパターンファイル形式の記述

README には「Life 1.05 (.LIF/.LIFE)」と書かれているが、実装は次のとおりである。

- 拡張子 `".lif"` だけを、**大文字と小文字を区別して** 比較している (`std::filesystem::path::operator==`)。
- `.LIF` や `.life` は読み込まれない。

あわせて次の点も確認した。
- パターンの並び順は `directory_iterator` の列挙順に依存する (NTFS では名前順になるが、保証はされていない)。キーの割り当て (`0`–`9`, `a`–`z`, `A`–`J`) を安定させたいなら、明示的にソートすること。
- `CellData` のファイル数は 46 で、README の記述と一致している。ただし `User.lif` は空のファイルで、選ぶと盤面が空になる。

### 3.13 [低] Win32 ラッパー (`ShosWin32.h`) — 無効化されている経路の不具合

現在は `IDLE` だけが有効なので実害はないが、教材として有効化すると動かない箇所がある。

| 場所 | 内容 |
|---|---|
| `ShosWin32.h:268-269` | `KillTimer` が `::SetTimer` を呼んでいる |
| `ShosWin32.h:441-445` | ウィンドウに関連付けたタイマーを `::KillTimer(nullptr, handle)` で止めているため、止まらない。ウィンドウハンドルとタイマー ID を保持する必要がある |
| `Shos.LifeGame.cpp:65` | `new Timer(*this, timerTimeoutMilliseconds)` の第 2 引数は `timerId` なので、100 ms のつもりが ID=100、周期は 1000 ms になる |
| `ShosWin32.h:237` | `vector<UINT>` に `std::` がなく、`<vector>` と `<algorithm>` も include していない。`USERMESSAGE` を有効にするとコンパイルエラーになる |
| `ShosWin32.h:404-416` | `WNDCLASSEXW` を明示しているのに、`GetClassInfoEx` / `RegisterClassEx` は TCHAR のマクロを使っている。マルチバイト構成ではビルドできない |
| `Shos.LifeGame.cpp:199` | `_tWinMain` の引数が `LPWSTR` になっている (`LPTSTR` が正しい) |
| `ShosWin32.h:477-487` | `IDLE` モードでは CPU コアを 1 つ 100% 使い続ける (ベンチマーク目的なら妥当)。最小化中も計算を続ける |
| `Shos.LifeGame.cpp:125` | 毎世代 `SetWindowText` を呼んでいる。数百 FPS ではそれなりのコストになるので、更新を間引くことを検討する |

### 3.14 [低] C++ としての品質

- `Window::GetHandle()` の `this == nullptr` (`ShosWin32.h:242`) は未定義動作で、最適化によって消されることがある。
- `BitCellSet`, `Board`, `Game` は生ポインタを所有しているのに、コピーコンストラクターと代入演算子を `= delete` していない。誤ってコピーすると二重解放になる。`std::unique_ptr` や `std::vector` に置き換えるのがよい。
- `ThreadUtility::ForEach` は世代ごとにスレッドを生成して join している。生成コストは 1 スレッドあたり数十 µs なので、244 FPS 程度では無視できない割合になる。スレッドプールや `std::for_each(std::execution::par, ...)` の利用を検討する。
- `[=]` による `this` の暗黙キャプチャ (`ShosLifeGame.h:917`, `:925`) は C++20 で非推奨になった。`[=, this]` と書くこと。
- `#endif !FAST` (`ShosLifeGame.h:827`) の `#endif` の後ろの余分なトークンで警告 C4067 が出る。`// !FAST` とコメントにすること。
- `Pattern::alive` は各インスタンスが持つ `const char` メンバーになっている。`static constexpr` にすべきである。
- `Random::Next()` は `mt19937` の値 (`uint32_t`) を `int` で返しているので、負の値になりうる。現在は `% 2` で使っているだけなので問題ない。
- `GetPatternName` や `SetPattern` で `int` と `size_t` を比較しているため、符号付き/符号なしの比較の警告が出る。
- `GetAliveNeighborCount` は 1 セルごとに `IsIn` と `ToIndex` を 8 回ずつ呼ぶ。`USEBITS` の利点 (ビット並列処理) を活かし切れていない。さらに高速化するなら、64bit 単位の SWAR 加算や、行ごとの累積和で近傍を数える方式が有効である。

### 3.15 [低] `ShosHelper.h` / `ShosStopwatch.h`

- `std::function`, `assert`, `std::max_element`, `std::back_inserter`, `std::transform` を使っているのに、`<functional>`, `<cassert>`, `<algorithm>`, `<iterator>` を include していない。include の順番が変わるとビルドが壊れる。
- `tstring` を `#define` で定義しているので、すべての名前空間を汚染する。`using tstring = std::basic_string<TCHAR>;` にすべきである。
- `String::Trim` は空白しか除去せず、タブや `\r` が残る。`std::string` に対して `tstring::npos` を使っている (値は同じなので動作はする)。
- `File::Read` は出力先の `lines` をクリアしない (`Map` や `Filter` はクリアしている)。
- README に書かれている `File` の書き込み機能と `String` の Join は実装されていない。
- `stopwatch` は `system_clock` を使っている (README には `high_resolution_clock` と書かれている)。計測には `steady_clock` が適切である。ミリ秒に切り捨ててから秒に変換している。
- `stopwatch_viewer::output` は定義されているが使われていない。
- 型名 `stopwatch` とメンバー名 `stopwatch` が同じ名前になっている (`MainWindow`, `stopwatch_viewer`)。

### 3.16 [低] テスト

`Shos.LifeGame.Test` は 2048×2048 の盤面を 100 世代進めて時間を表示するだけのベンチマークで、正しさを検証していない。また、コメントに書かれた結果 (`23.363s` など) は、現在の構成や記事のデータ (1000×1000) と条件が異なる。

**推奨するテスト**
- 既知のパターンでの検証: ブリンカー (周期 2)、ブロック (固定)、グライダー (4 世代で (1,1) 移動)
- 4 つの `#define` の全組み合わせで同じ結果になることの比較
- パーサーの単体テスト: RLE (`n$`, 数字の連続、`!` なし)、Life 1.05 (`!` 行、空行)
- 盤面の端の扱い (端はトーラスではなく「外側は死」であることの確認)

あわせて、GitHub Actions (`windows-latest` + MSBuild) で 16 通りの `#define` の組み合わせをビルドするマトリクスを作ることを勧める。現状では、使われない組み合わせが壊れていても気付けない。

### 3.17 [低] ドキュメントとリポジトリの構成

- `Articles(Japanese)/ShosLifeGame_Article.md`
  - 28 行目: 見出しと本文が結合している (`### 最適化の結果図3はこの責務分担を1枚で示した地図として使ってほしい。`)。Qiita 版では修正済み。
  - 109 行目: 文が途中で切れている (`ライフゲームでは、さまざまなパターン`)。
  - 本文で `TRUE`/`FALSE` と `定義`/`未定義` が混在している。`#define` の説明としては「定義/未定義」に統一するのが正確である。
- README
  - `&quot;` の HTML 実体参照は不要なので、普通の `"` で書ける。
  - `Helper` の説明 (文字列と数値の変換、時間計測) が実装と一致していない。
  - 端の扱い (トーラスではない) やキー操作 (右クリックでランダム化) の記載がない。
- 画像ファイル名の綴りが `lifefame01...` になっている (`lifegame` の誤り)。同じ画像が `Images/` と `Articles(Japanese)/Images/` に重複している。
- `Shos.LifeGame/all.zip` (7809 ファイル、展開後約 23 MB) がコミットされている。出典とライセンスを README に明記するか、リポジトリの外 (Release の添付ファイルなど) に移すことを勧める。
- `Images/*.original.mp4` や `ShosLifeGame_BenchmarkData.old.csv`、記事の Draft/Plan 版など、作業途中のファイルが残っている。公開リポジトリとして整理するかどうかを検討する。

---

## 4. 優先して対応すべき事項 (推奨順)

1. `.git` の破損への対処 (clone し直し、Dropbox の同期対象から外す) — 3.1
2. `Board` のデストラクタの修正、または `USEBITS` を既定で有効にする — 3.2, 3.11
3. パターンパーサーの修正 (RLE の `n$`、Life 1.05 の `!` 行と空行) — 3.3, 3.4
4. ビット順の実機確認と修正、`unitNumberX` の修正 — 3.5, 3.6
5. 既知のパターンによる回帰テストと、CI での `#define` 全組み合わせのビルド — 3.16
6. `AREA` の範囲が縮まない件を記事に注記する、または範囲を縮められるようにする — 3.9

## 5. 良い点

- 最適化の要素を `#define` で独立に切り替えられ、段階ごとのベンチマーク (CSV + 記事の図 10) で効果を定量的に示している。教材として優れている。
- `MT` と `AREA` を組み合わせたとき、各スレッドが自分の範囲 (`areas[index]`) を持ち、最後に `Rect::Union` でまとめている。共有状態をロックなしで扱える設計になっている。
- 行単位でスレッドに分割しているので、ビット表現 (1 行が独立したバイト列) でもスレッド間でデータ競合が起きない。
- Debug ビルドでは、`Game::Test` で「範囲の外に生きているセルがない」ことを毎世代 `assert` で検証している。
- Win32 ラッパーが小さく、メッセージループと仮想関数ディスパッチの仕組みを学ぶのに適した分量になっている。
