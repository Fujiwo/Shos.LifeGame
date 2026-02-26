# 図10 作成手順(ShosLifeGame_BenchmarkData.csv から作図)

対象データ: `ShosLifeGame_BenchmarkData.csv`

## 目的
- 記事の図10(ベンチ結果グラフ)を、同一データから再現可能に作成する。
- 画像形式は `PNG`(記事貼り付け用)または `SVG`(編集用)を推奨する。

## 作図手順(Excel)
1. `ShosLifeGame_BenchmarkData.csv` をExcelで開く。
2. 列 `Optimization Level` と `Time (seconds)` を選択する。
3. 「挿入」→「縦棒」→「集合縦棒」を選択する。
4. グラフタイトルを次に変更する。  
   `Shos.LifeGame ベンチ結果(Optimization Level 0〜4)`
5. 縦軸タイトルを次に変更する。  
   `実行時間(秒)`
6. データラベル(値)を表示する。
7. 画像として保存する。
   - SVGまたはPNGで保存
   - 推奨ファイル名: `ShosLifeGame_Figure10_Benchmark.svg`
   - 推奨保存先: `Articles(Japanese)/Images/`

## 作図手順(Python: 任意)
以下は再現用サンプルです(実行は任意)。

```python
import pandas as pd
import matplotlib.pyplot as plt

csv_path = "ShosLifeGame_BenchmarkData.csv"
out_path = "Images/ShosLifeGame_Figure10_Benchmark.svg"

df = pd.read_csv(csv_path)

plt.figure(figsize=(7, 4))
plt.bar(df["Optimization Level"].astype(str), df["Time (seconds)"])
plt.title("Shos.LifeGame ベンチ結果(Optimization Level 0〜4)")
plt.xlabel("構成")
plt.ylabel("実行時間(秒)")

for i, v in enumerate(df["Time (seconds)"]):
   plt.text(i, v, f"{v:.1f}s", ha="center", va="bottom")

plt.tight_layout()
plt.savefig(out_path, dpi=200)
plt.close()
```

## 記事へ反映する注記テンプレート
図10のキャプションまたは本文に、以下を併記する。

- 測定元: `Articles(Japanese)/ShosLifeGame_BenchmarkData.csv`
- 測定条件: Pattern `Gun_Gunstar`、盤面サイズ `1000x1000`、反復 `1000` 回
- 注意: CPU・ビルド設定(Debug/Release)・環境差で値は変動する

## 推奨ファイル配置
- CSV: `Articles(Japanese)/ShosLifeGame_BenchmarkData.csv`
- 手順書: `Articles(Japanese)/ShosLifeGame_Figure10_Guide.md`
- 出力画像: `Articles(Japanese)/Images/ShosLifeGame_Figure10_Benchmark.svg`
