# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 概要

C++20 と Win32 API だけで書いた高速ライフゲーム (B3/S23)。最適化の要素を `#define` で切り替えて効果を比べる教材であり、`Articles(Japanese)/` の解説記事とコードが対応している。

## ビルドとテスト

- **アプリ (Windows)**: Visual Studio で `Shos.LifeGame.sln` を開き、x64 の Debug/Release でビルドする (ツールセット v145、C++20)。ビルド後のイベントで `CellData/` が robocopy により出力先へコピーされる。
- **回帰テスト (Linux/WSL, g++)**: `USEBITS`/`FAST`/`MT`/`AREA` の 16 通りすべてをビルドして実行する。CI (`.github/workflows/regression-tests.yml`) も同じスクリプトを実行する。
  ```
  ./Shos.LifeGame.Test/run_regression_tests.sh          # 約 2 分
  ./Shos.LifeGame.Test/run_regression_tests.sh --asan   # ASan/UBSan 付き。1 通りで約 1 分かかる
  ```
- **1 つの組み合わせだけを試す場合**:
  ```
  g++ -std=c++20 -O1 -pthread -D_DEBUG -DSHOS_LIFEGAME_CUSTOM_CONFIG -DUSEBITS -DFAST -DMT -DAREA \
      -IShos.LifeGame.Test/LinuxCompat Shos.LifeGame.Test/Shos.LifeGame.RegressionTest.cpp -o /tmp/t \
      && /tmp/t Shos.LifeGame/CellData
  ```
  個々のテストを選んで実行する仕組みはない。`Run()` のテスト一覧で、対象外の行をコメントにする。
- `Shos.LifeGame.Test/Shos.LifeGame.Test.cpp` (Visual Studio のテストプロジェクト) は、正しさを検証しないベンチマークである。
- Linux でビルドできるのは `ShosLifeGame.h` と `ShosHelper.h` だけである。`ShosWin32.h`、`ShosLifeGameBoardPainter.h`、`Shos.LifeGame.cpp` は MSVC でしかコンパイルできない。これらを変更した場合は、Windows での確認が必要であることを利用者に伝える。

## アーキテクチャ

### 最適化の `#define` (`ShosLifeGame.h` の冒頭)
- `USEBITS`: `Board` が `BitCellSet` (1 セル 1 ビット) を継承する。未定義の場合は、`bool**` を使う別実装の `Board` になる。2 つの `Board` は同じインターフェイス (`Get`/`Set`/`SetOnly`/`GetArea`/`SetArea`/`GetBits`) を持つ必要がある。
- `FAST`: `std::function` を使う汎用ループ (`Utility::ForEach`) の代わりに、直接のループを使う。
- `MT`: 計算する範囲を行 (Y 方向) で分け、スレッドごとに `NextPart` を実行する。`AREA` も有効なら、スレッドごとに範囲 (`areas[index]`) を持ち、最後に `Rect::Union` でまとめる。
- `AREA`: 活動範囲だけを計算する。
- `SHOS_LIFEGAME_CUSTOM_CONFIG` を定義すると、冒頭の `#define` が無効になり、外部 (テスト) から組み合わせを指定できる。
- コードを変えるときは、16 通りすべてで回帰テストが通ることを確認する。どの経路も `#if` で分かれているので、使っていない組み合わせが壊れても気付きにくい。

### `Game` の世代更新と `AREA` の不変条件
- `mainBoard` (現在の世代) から `subBoard` (次の世代) を計算し、`std::swap` で入れ替える。
- 計算するのは、`mainBoard` の範囲の中だけである。そのため、**「各盤面で、範囲の外のセルはすべて死んでいる」** という不変条件が正しさを支えている。
- 範囲は、世代ごとに「空の範囲 (`BitCellSet::GetEmptyArea`)」から始め、生きているセルを `Union` で足していく。その結果は、外接矩形に周囲 1 マスを加えたものになり、範囲は縮むこともある。範囲が縮んだときは、計算の前に `Game::ClearOutside` で古いセルを消す。生きているセルが 1 つもなければ、`GetDefaultArea` (盤面中央の 3×3) に戻す。
- Debug ビルド (`_DEBUG`) では、`Game::Test` が毎世代この不変条件を `assert` で確かめる。テストスクリプトは `-D_DEBUG` を付けてビルドする。

### ビット配置と描画の約束 (`BitCellSet` と `BoardPainter`)
- `GetBits()` が返すビット列は、そのまま 1bpp のモノクロビットマップとして `CreateBitmapIndirect` に渡される。
- そのため、各バイトの最上位ビット (MSB) を左端のピクセルにする (`ToIndex`)。また、1 行のバイト数は WORD 境界にそろえる (`BitCellSet::GetRowByteNumber`)。
- ビット配置を変える場合は、`ToIndex`、`GetRowByteNumber`、`bmWidthBytes` の 3 か所をそろえて変える。テスト `BitmapMatchesBoard` は、この約束を検証している。

### パターンファイル (`PatternSet`)
- 作業ディレクトリーの `.\CellData` から読み込む。`_tWinMain` は起動時に、作業ディレクトリーを exe のフォルダーに変える。
- 並び順は、Life 1.05 (`.lif`/`.life`) が先、RLE (`.rle`) が後である。それぞれの中では、ファイル名を大文字と小文字を区別せずに並べる。
- この並び順に、キー `0`–`9`、`a`–`z`、`A`–`J` が割り当てられる。ファイルを追加したり名前を変えたりすると、キーの割り当てがずれる。
- Life 1.05 の扱い: `#` 行と `!` 行 (Plaintext 形式のコメント) は除外する。`#P x y` はブロックの配置座標として扱う。ブロックの途中の空行は「すべて死んでいるセルの行」であり、除外してはならない (例: `13_engine_Cordership.lif`)。
- RLE の扱い: `n$` は、行の確定と (n−1) 行の空行として扱う。末尾の `!` は省略されることがあり、末尾の空行が省略された場合はヘッダーの `y` まで空行で埋める。ヘッダーの `rule` は無視する。

### Win32 ラッパー (`ShosWin32.h`)
- メッセージループの方式は、`IDLE`/`TIMER`/`USERMESSAGE` の `#define` で選ぶ。有効なのは `IDLE` だけで、`PeekMessage` が空のたびに `OnIdle` で 1 世代を進める。そのため CPU を 1 コア使い続けるが、これは FPS を計測するための意図した動作である。
- タイトルバーに表示する FPS は、記事のベンチマークの数値の出どころである。描画や `SetWindowText` の頻度を変えると、記事の数値と比べられなくなる。

## テストの書き方

- テストは `Shos.LifeGame.RegressionTest.cpp` にある、外部ライブラリを使わない自前の仕組みで書く。期待値には、単純な参照実装 (`ReferenceGame`) との比較を使う。
- 既知の不具合を記録するテストは `Expect::KnownBug` にする。このテストは、失敗すると XFAIL (想定どおり)、通ると XPASS (テスト全体が失敗) になる。不具合を直したら `Expect::Pass` に変える。
- ソースファイルとパターンファイルの改行は CRLF である。編集するときは、改行コードを保つ。

## 記事 (`Articles(Japanese)/`)

- `Articles(Japanese)` フォルダー内で記事を書く・改稿する・推敲する場合は、スキル `.claude/skills/fujiwo-manuscript-voice` を用いる。
- `ShosLifeGame_Article.md` (本体) と `ShosLifeGame_Article.Qiita.md` (Qiita 版) は、内容をそろえて両方を更新する。Qiita 版は、画像を GitHub の raw URL で参照している点が本体と違う。リポジトリ内の Qiita 版を更新しても、Qiita に公開されている記事は変わらない。
- 記事のベンチマーク (表と図 10、`ShosLifeGame_BenchmarkData.csv`) は Windows で測った値である。性能に影響する変更をしても、数値を推測で書き換えてはならない。測り直しが必要であることを利用者に伝える。
- 記事では、`#define` の状態を「定義/未定義」と書く (`TRUE`/`FALSE` とは書かない)。

## その他

- `Reviews/` には、リポジトリのレビュー結果がある (`2026-10-13_RepositoryReview.md`)。指摘への対応状況は、Git の履歴で確認する。
