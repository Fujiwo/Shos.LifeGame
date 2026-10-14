# 依頼: 高速化の技法と効果の解説を README と記事に追加する

## 背景
speedup ブランチで、`Plans/2026-10-14_SpeedupPlan.md` の Step 0〜6 の高速化を実装した。
README.md、README.jp.md、`Articles(Japanese)/` の記事では、各技法の説明が 1〜2 行の表や短い節にとどまっている。
そのため、「なぜ速くなるのか」と「どれだけ速くなるのか」が読者に伝わらない。

## 正とする情報源
- 実装: `Shos.LifeGame/ShosLifeGame.h`、`Shos.LifeGame/ShosLifeGameBoardPainter.h`
- 高速化の経緯と計測値: `Plans/2026-10-14_SpeedupPlan.md` (6 章「計測結果」)
- 記事のベンチマーク: `Articles(Japanese)/ShosLifeGame_BenchmarkData.csv` (Windows で測った値)
- 説明はコードから確かめられる事実だけで書く。コードと食い違う既存の説明を見つけたら、直したうえで報告する。

## 解説する技法
1. `USEBITS`: 1 セル 1 ビット (`BitCellSet`) と、MSB を左端にするビット配置
2. `FAST`: `std::function` を使わない直接ループと、行単位の計算 (`Game::NextRow`、列ごとの 3 セルの和の共有、結果のバイト単位の書き込み)
3. `MT`: 行での分割、`ThreadPool` によるスレッドの常駐、`std::barrier` による同期、`minimumCellNumberPerThread` によるスレッド数の調整
4. `AREA`: 活動範囲だけの計算、範囲の縮小、`ClearOutside`、不変条件「範囲の外のセルはすべて死んでいる」
5. 描画: `SetBitmapBits` + `BitBlt`、ビットマップの使い回し、タイトル更新の間引き
6. その他: `Randomize` の改善など、Step 6 の小さな改善

## 各技法について書く項目
- (a) 解決する問題: 最適化しない場合に、どこで何が遅いのか
- (b) 考え方: 図か ASCII 図で示せるものは示す (例: ビット配置、3 セルの和の共有、行の分割、活動範囲)
- (c) 実装: 該当する関数名と、要点だけを抜き出した短いコード (10〜20 行程度)
- (d) 効果: 計測値と、その計測の条件
- (e) 代償と、効果が小さくなる条件 (例: `AREA` はランダムな盤面では効きにくい)
- (f) 他の技法との組み合わせによる効果の違い (例: `MT` と `AREA` を併用するときの `areas[index]` と `Rect::Union`)

## 数値の扱い (必ず守ること)
- Windows のアプリの FPS (README の Benchmark 表、記事の表と図 10) は書き換えない。推測で数値を作らない。
- Step 0〜6 の効果を示す場合は、`SpeedupPlan.md` の WSL/g++ での `Game::Next` の計測値を使う。そのとき、Windows の FPS とは条件が違うことを明記する。
- Windows での測り直しが必要になる箇所には、その旨を書き、完了時に一覧で報告する。

## 作業 1: README.md (英語版)
- 「Optimizations」節を、上の項目 (a)〜(f) で技法ごとの小節に書き直す。既存の要約表は、概要として冒頭に残す。
- 「Benchmark」節で、各レベルの伸び (×1.06、×2.56 など) がどの技法のどんな性質から来るのかを説明する。
- 平易な英語で書き、1 文には 1 つのことだけを書く。

## 作業 2: README.jp.md (日本語版)
- 作業 1 の完了後に、README.md と節の構成と内容をそろえる。直訳調にはせず、自然な日本語で書く。
- 両方のファイルで、見出しの対応とリンク先 (アンカーを含む) が壊れていないことを確かめる。

## 作業 3: `Articles(Japanese)/` の記事
- 対象: `ShosLifeGame_Article.md` (本体) と `ShosLifeGame_Article.Qiita.md` (Qiita 版)。両方の内容をそろえる。Qiita 版の画像は GitHub の raw URL のまま残す。
- 対象外: `ArticleDraft.md`、`ArticlePlan.md` などの作業用ファイル。ただし、`ChapterMapping.md` と `ConsistencyCheck.md` が本文と食い違った場合は更新する。
- 主に 5 章「高速化テクニック4要素と描画最適化の実装読解」を、(a)〜(f) で書き直す。必要なら 4 章と 6 章も直す。
- 執筆にはスキル `fujiwo-manuscript-voice` を使う。`#define` の状態は「定義/未定義」と書く。
- 新しい図が必要な場合は、Markdown の中に Mermaid か ASCII 図で書く。Mermaid や ASCII 図が向かないものについては、SVG や PNG で描く

## 共通の約束
- 改行コード (CRLF) を保つ。
- ソースコードは変更しない。記事に必要なコードの不具合を見つけた場合は、報告するだけにする。
- 作業 1、2、3 の順に進める。作業ごとに差分の要点を報告し、私の確認を待ってから次の作業に進む。コミットはしない。
- 作業 1〜3 を一度に進める
- 記事の図は Mermaid か ASCII 図で描くが、Mermaid や ASCII 図が向かないものについては、SVG や PNG で描く
- 記事だけでなく README も詳細に書く (README にも項目 (a)〜(f) を書く)

## 完了時の報告
- 変更したファイルと、節ごとの変更の要約
- Windows での測り直しが必要な箇所の一覧
- コードと食い違っていて直した既存の記述の一覧
