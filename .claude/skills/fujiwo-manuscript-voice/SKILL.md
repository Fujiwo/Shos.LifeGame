---
name: fujiwo-manuscript-voice
description: "小島富治雄(Fujiwo)名義の書籍・記事の日本語 Markdown 原稿を新規執筆・改稿・診断するときに使う。「Fujiwoらしく」「人が書いたように」「AIっぽさを消して」「意味を変えずに読みやすくして」という依頼や、原稿の文体統一、主述・修飾・接続の推敲に対応する。"
argument-hint: "[write|revise|audit] [quick(既定)|full] 対象の原稿ファイル"
---

# fujiwo-manuscript-voice

Fujiwo 名義の書籍・記事の日本語 Markdown 原稿を、本人が書いたと読める文章にするための入口スキル。既存4スキルを中核とし、`yomiyasu` を局所的な推敲と意味保持の点検に使う。各スキルの内容は転記せず、**どの順で読み、どの工程に適用し、衝突したらどちらを採るか**をここで固定する。

- `../fujiwo-style/SKILL.md` : 表記・語尾・構成・Fujiwo らしさの実測ルール。**最終権限**
- `../japanese-tech-writing/SKILL.md` : 段落構成・論証の厳密さ・読み手の負荷管理・翻訳調の比喩や擬人化の禁止を定める**論証の規範**
- `../natural-japanese/SKILL.md` : 設計→執筆→検査→収束→音読の**工程の骨格**と読みやすさ原則
- `../stop-ai-slop-jp/SKILL.md` : AI臭を見つける**検査のレンズ**。書籍・記事の原稿向けに絞って使う
- `../yomiyasu/SKILL.md` : 主述・修飾・接続を整え、修正前後の意味を比べる**補助スキル**。独立した改稿工程は回さない

適用範囲は、執筆プロファイルが対象と定める原稿の Markdown だけである。議事録、計画書、社内文書など原稿でない文書には使わない(それぞれ元の4スキルを直接使う)。

## 0. 執筆プロファイル

出版社や媒体、プロジェクトごとに変わる約束事は、このスキルには書かず、プロジェクトの**執筆プロファイル**に書く。このスキルは、プロファイルを読んでから動く。

- **探す順序**: (1) 依頼者が指定したファイル、(2) リポジトリの指示ファイル(`CLAUDE.md`、`.github/copilot-instructions.md` など)が指すファイル、(3) リポジトリのルートの `執筆プロファイル.md`。(2)と(3)が食い違うときは(2)を採り、食い違いを最終報告に書く。
- **優先順位**: 依頼者がその場で明示した指示 > プロファイル > 本スキルの裁定表(`references/conflict-resolutions.md`)と中核4スキル・補助スキル。表記・文体の優先順位は、読みやすさの修正で意味を変えてよいという許可ではない(裁定 #22)。
- **プロファイルに書く項目**: 対象ファイルと媒体(紙・電子・Web)、参照資料(目次、計画書、本人の声の正例)の場所、読者像と説明の粒度、文体の基調と要素ごとの文体(定型部、注記など)、自称と呼びかけ、見出しタグと章の定型、表記・書式(コード、図版、リンク、出典、注記)と禁止事項、本人の声の具体例、lint から除外する定型行、プロジェクト固有のチェック項目、検証コマンド。
- **新しいプロジェクトへの導入**: スキルのコピー、プロファイルの書き方、動作の確かめ方は `references/adoption-guide.md` にある。プロファイルのひな形は `references/profile-template.md`。
- **プロファイルがないとき**: 文体は常体基調とする(fujiwo-style 4節 型Dの既定)。読者像・見出しタグ・定型部・書式は、既存の原稿から推測で決めず、依頼者に確認する。確認できないまま `revise` / `audit` を行う場合は、見出しや定型に見える部分を変えない側に倒し、最終報告でその旨を書く。

## 1. 先に読むもの

1. 執筆プロファイル(0節)。
2. `../fujiwo-style/SKILL.md` 全文(短い)。構成の型は 4節から選ぶ(書籍の章なら型D。プロファイルに指定があればそれに従う)。
3. `../japanese-tech-writing/SKILL.md` 全文。段落・論証・読み手の負荷・比喩の規範として使う。
4. `../natural-japanese/SKILL.md` と、原稿の種類に合う `../natural-japanese/references/doctypes/`(書籍・記事なら `book.md`)。
5. `../stop-ai-slop-jp/SKILL.md` の「大原則」「コアルール」「修正の優先順位」。
6. `../yomiyasu/SKILL.md` と `../yomiyasu/references/domains/tech.md`。意味保持と局所的な推敲に使い、数値目安・文体・出力形式は本スキルの裁定に従う。構文の判断に迷ったときだけ `../yomiyasu/references/gemini-syntax.md` と `../yomiyasu/references/slop-catalog.md` の該当箇所を読む。
7. 本スキルの `references/conflict-resolutions.md`(中核4スキルと補助スキルの衝突と裁定)、`references/yomiyasu-integration.md`(適用範囲と検査手順)。**衝突が見えたら自分で裁定せず、この表を引く。表にない衝突は fujiwo-style を採り、最終報告で報告する。ただし意味が変わる修正は適用せず、確認事項にする。**
8. プロファイルが指す、対象章の計画書と目次(あれば)。

Fujiwo 本人の書籍調の正例は、プロファイルが指す資料(自筆メモ、原稿中で `>` 引用ブロックになっている元原稿・公開記事の引用など)にある。「人が書いた感じ」に迷ったら、これを読み直す。stop-ai-slop-jp の言う「毒・自虐」は、**Fujiwo 流の軽口と具体的な脱線**に読み替える(具体例はプロファイル)。罵倒・皮肉・「めっちゃ」「ガチで」のような口語は採らない。

## 2. モード

引数がなければ対象から判断する。

- `write` : 新規の章・節を書く。本スキル 3-1(設計)から始める。
- `revise` : 既存の章を改稿する(既定)。3-1 は計画書の確認で済ませ、3-3 の検査から入る。
- `audit` : 書き換えずに診断だけ返す。natural-japanese の `score` 相当に、`references/manuscript-checklist.md` の「立場」「主体性」の所見を添える。

`yomiyasu` は、`revise` では修正前後の意味保持、`write` では提供素材と草稿の対応、`audit` では読みやすさの問題と意味が変わりうる修正案の診断に使う。`audit` で本文を書き換えたり、診断用に原稿を保存し直したりしない。

`quick`(既定)/`full` の使い分けは natural-japanese の「実行モード」に従う。1章まるごとの改稿は 1万字を超えるので `full` を提案する。

## 3. 工程

### 3-1. 設計(write のとき。revise では確認だけ)

- 読者像はプロファイルに従う。fujiwo-style 1節の2つの読者像(実務経験者向け/学習者向け)のどちらの説明粒度を採るかも、プロファイルで決まる。
- プロファイルが定める章の定型と見出しタグは崩さない。**見出しの文言は目次と計画書で決まっている。本スキルの対象外**(裁定 #1)。
- 主メッセージを一文で書けるか確かめる。書けなければ計画書か参考資料に戻る(natural-japanese §1-2)。
- stop-ai-slop-jp の「大原則」を、章単位で1回だけ問う。この章で Fujiwo は何を引き受けて言い切るのか。反論できる具体的な主張は何か。答えが出ないまま表面を直しても AI 臭は残る。

### 3-2. 執筆

fujiwo-style 2〜4節(敬体/常体、文長、半角記号、課題→解決の型、型Dのリズム調整)、japanese-tech-writing の段落構成・論証の一方向性(結論→反論処理→結論の言い直しにしない)、natural-japanese の文体憲法(`references/writing-constitution.md`)の下で書く。この段階で細かい禁止語は気にしない。検査工程が拾う。

文体の基調(常体基調か敬体基調か)は、このスキルでは固定しない。依頼者の指示か、プロファイルの指定に従う。両者が食い違うときは依頼者の指示を採る。どちらにも指定がなければ、fujiwo-style 4節 型Dの既定どおり常体基調とする。敬体基調のときは fujiwo-style 2節「敬体基調(指定がある場合)」に従う。定型部や注記など、要素ごとの文体もプロファイルに従う。どちらの基調でも、見出しの文言(裁定 #1)、`>` の引用、コード内のコメントの文体は変えない。リズム調整は、fujiwo-style 4節 型Dのうち採った基調の規定に従う(裁定 #14、#21)。

### 3-3. 検査(1) 機械

リポジトリのルートで実行する。`<skills>` は、このスキルが置かれているスキルディレクトリ(`.github/skills`、`.claude/skills`、ユーザー単位の場所など。実際の場所に読み替える)。`uv` がなければ `python` で直接呼ぶ。`sudachipy`または`sudachidict_core`の`ModuleNotFoundError`が出た場合は、同じPython環境へ`python -m pip install sudachipy sudachidict-core`で導入してから再実行する。

```bash
# bash(JSON は一時ファイルへ。NN は任意の番号)
export PYTHONUTF8=1
python <skills>/natural-japanese/scripts/lint.py --genre tech <対象ファイル>
python <skills>/natural-japanese/scripts/lint.py --genre tech --json <対象ファイル> > "$TMPDIR/lint-NN.json"
# 修正後: 前回 JSON と比較して resolved / new / persisting を仕分ける
python <skills>/natural-japanese/scripts/lint.py --genre tech --json --baseline "$TMPDIR/lint-NN.json" <対象ファイル>
```

```powershell
# PowerShell(Windows では文字コード指定が必須。指定しないと --baseline で読めない)
$env:PYTHONUTF8=1; [Console]::OutputEncoding=[Text.Encoding]::UTF8
python <skills>/natural-japanese/scripts/lint.py --genre tech <対象ファイル>
python <skills>/natural-japanese/scripts/lint.py --genre tech --json <対象ファイル> | Out-File -Encoding utf8 $env:TEMP\lint-NN.json
python <skills>/natural-japanese/scripts/lint.py --genre tech --json --baseline $env:TEMP\lint-NN.json <対象ファイル>
```

見出し・箇条書き・引用・表・コードブロックは lint 側で自動的にマスクされるが、原稿固有の定型行が誤検知を生む。**`references/exclusions.md` の汎用の除外と、プロファイルの除外表に挙げた finding は「残す(定型)」として台帳に一行で処理し、直さない。**

### 3-4. 検査(2) 目視

`references/manuscript-checklist.md` を上から順に回し、続けてプロファイルのプロジェクト固有のチェック項目を回す。チェックリストは stop-ai-slop-jp のクイックチェックから書籍・記事の原稿に合わない項目を外し、fujiwo-style 7節・japanese-tech-writing・book.md の品質基準を足したものである。優先順位は **立場 > 論証の厳密さ > 主体性(false agency・翻訳調の比喩や擬人化) > 読み手の負荷管理 > 構造のテンプレ > 語彙 > 記号**。記号だけ直して終えない。

natural-japanese §4 の構造レビュー(`scripts/outline.py`)と読みやすさレビュー(`references/readability-antipatterns.md`)もここで行う。`full` のときはサブエージェントに分けてよいが、「直す/残す」の判断は親が行う。

読みやすさレビューでは `yomiyasu` の主述・修飾・接続の点検を併用する。進め方は `references/yomiyasu-integration.md` 2節に従う。指摘は既存の台帳へ統合する。

### 3-5. 収束と台帳

finding ごとに「直した」か「残す(理由)」を台帳に書く。この原稿では**「残す」の行を必ず残す**こと。AI 臭を消す目的で全段落を一律に直すと、元原稿の濃淡が消えて逆に AI 臭が増す(natural-japanese `revision-guide.md`「改稿を一律に適用しない」)。特に次は一律置換しない。

- `ではなく` : 本当の誤解訂正(「AはBではなくCだ」と読者が誤解しやすい箇所)だけ残し、残りは直接 Y を書く(裁定 #11)
- `**用語**` : 初出の用語太字は残す。強調のための太字は節あたり1、2箇所まで(裁定 #9・#17)
- 同じ形の文末(「〜である。」「〜です。」など)の連続 : 4文以上続く箇所だけ崩す。すべての文末をばらけさせようとしない

意味保持は、修正前の本文を作業専用の一時領域に保持し、変更箇所ごとに `yomiyasu` の「主張・比重・言い切りの強さ・文の働き」を比べて確かめる。比較基準、`yomiyasu_lint.py` / `yomiyasu_diff.py` の実行条件とコマンド、承認済みの内容変更の扱い、意味を保つ案が得られないときの対応は `references/yomiyasu-integration.md` に従う。補助スキル用の台帳・出力形式・収束ループは増やさない。

全体の収束は natural-japanese の工程で管理する。lint を再実行し、新しい finding が出なくなるまで 3-3〜3-5 を繰り返す。ただし同じ指摘が2周連続で再発する場合は発散ガードを適用する。`yomiyasu` の警告を消すためだけの反復修正はせず、原文を残すか著者確認へ進む(裁定 #29)。

### 3-6. 最終パス

音読する(声に出すつもりで通読)。fujiwo-style 7節「執筆後に確認する項目」で締める。全角 `（）！？．，` が残っていないかは最後にもう一度だけ検索する(プロファイルが全角を指定している記号は除く)。プロファイルの検証コマンドを実行する。

### 3-7. 後片付けと報告

台帳・lint の JSON・下書き・意味保持の比較用ファイルは削除する。元の原稿や利用者が用意した資料は削除しない。報告には次を含める。

- 直した箇所の種類と件数、「残す」と判断した主な finding と理由
- 意味保持の比較基準、確認した変更の種類、原文を残した不明点。承認済みの内容変更と読みやすさの修正を区別する
- `conflict-resolutions.md` にない衝突に出会ったか(あれば裁定案とともに報告し、表への追記をユーザーに提案する)
- プロファイルが見つからなかった、または必要な項目がなかった場合は、その旨と、どう扱ったか
- 確認できなかったこと(音読は AI にはできないので、「リズムは黙読で確認した」と正直に書く)

## 4. やらないこと

- 見出しの文言変更、章の定型部の文言の書き換え、プロファイルが定める見出しタグの変更
  - 「書き換え」は文言(内容)を変えることで、文体の変更は含まない。定型部の文末を文体の基調に揃えるのは、3-2の規定どおり行ってよい
  - japanese-tech-writing の「見出しの付け方」は `write` で新規に見出しを立てるときだけ適用し、既存章の見出しには適用しない(裁定 #16)
- 中核4スキル・補助スキルの規則をこのファイルに写して独自に改変すること(元スキルを更新したら、`conflict-resolutions.md` の裁定と `references/manuscript-checklist.md` に転記済みの具体的な語彙・数値閾値の両方を見直す)
- プロジェクト固有の約束事をこのスキルに書き足すこと(プロファイルに書く)
- プロファイルが対象と定めていないファイルへの適用
- プロファイルが禁止している表現・図版の追加(例: 印刷物向けの原稿での Mermaid 図)
