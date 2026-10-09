# BatchOptimizer：一括方式の SA による個性特性の最適化

`job_server/` のバッチ推論サーバーを使って、BFI-2 の15下位特性を焼きなまし法（SA）で最適化する。
昨年の `runOptimizationExperiment`（逐次版・ファイルブリッジ）と `runOptimizationExperimentParallelHttp`
（2GPU 並列版）は残してあり、`BatchOptimizer` はそれらを使わない。

## 何をするか

1周 ＝ 全員を1回ずつ、周ごとにシャッフルした順番で判定する。

1. 順番をシャッフルし、その順番で全員の変更案（15下位特性に N(0, step) を足して [0,1] に収める）を作る
2. 全員分（403人 × 51問）のプロンプトを1ジョブとして `POST /jobs` で投げる
3. `GET /jobs/{id}` で done になるまで待つ
4. 順番どおりに採否を判定する（ΔTAE < 0 なら採用、そうでなければ確率 exp(-ΔTAE/T)）。冷却は1件ごと
5. チェックポイントを保存する

最初に sweep 0 として、初期個性（一様乱数）で全員を推論し、初期 TAE を求める。

## 使い方

```bash
cp config/batch_optimizer.example.json my_config.json   # 編集する
./build/src/BatchOptimizer my_config.json
```

- 相対パスは、実行したときのカレントディレクトリ（リポジトリのルートを想定）から解決される
- `client_id` は人ごとに変える（例：`bp22029`、後輩は自分のアカウント名）
- `enable_thinking`・`reasoning_effort`（`xhigh`・`medium`・`low`）・`max_tokens` は推論条件で、各自の研究で決めて書く。
  思考なし（`"enable_thinking": false`）のときは `reasoning_effort` を書かない。サーバーはジョブごとにこの条件で推論するので、
  2人が別の値を使ってもよい。最適化した個性はその条件でのものなので、個性を使う実験でも思考の有無と effort はできるだけ揃える
  （job_server/SETUP_BLACKWELL.md §7）
- 途中で止まっても、同じ設定ファイルで起動し直せばチェックポイントから再開する。
  サーバーに同じ周のジョブが残っていれば、投げ直さずにその結果を使う
- 途中で設定（推論条件、温度、周回数、seed、プロンプトなど）を変えて再開しようとすると止まる。
  サーバーのモデル・revision・vLLM などのバージョンが変わっていた場合も止まる

## 出力（`run_dir`）

| ファイル | 内容 |
|---|---|
| `checkpoint.json` | 完了した周、全員の個性と回答、TAE、温度、乱数生成器の状態、各周の job_id |
| `sweeps.csv` | 周ごとの TAE、温度、採用数、解析できなかった回答数、`finish_reason == "length"` の件数 |
| `sweeps/sweep_NNN_decisions.csv` | その周の1人ごとの判定（順番、ΔTAE、採否、温度） |
| `sweeps/sweep_NNN_population.csv` | その周の後の個性と回答（昨年の merged_population と同じ列構成） |
| `server_info_*.json` | 起動ごとに記録したサーバー情報（`GET /info`） |

## 再現性のために

- 乱数は `std::mt19937_64` だけを使い、一様整数・一様実数・シャッフル・正規分布は自前で実装している
  （`portable_random`）。`std::*_distribution` と `std::shuffle` は処理系によって結果が変わるので使わない
- TAE は常に回答の件数から決まった順番で足し上げて計算する（`TaeTracker`）。差分の足し引きで
  更新しないので、再開しても通しで回した場合と同じ値になる
- 回答は、思考の後（最後の `</think>` の後）の最後の `<answer>N</answer>` を使う。
  選択肢の範囲外の番号と、`finish_reason == "length"` で打ち切られた出力は解析できなかったものとして扱い、
  その質問は前の回答のままにする。1人分すべて解析できなかった場合は、乱数を引かずに不採用とする
- ビルド環境は Docker（`Dockerfile`：Ubuntu 24.04 / g++ 13）で2人揃える。`-ffast-math` は使わない。
  `PortableRandomTest.FixedValuesForSeed42` が通れば、乱数列は記録した環境と一致している

```bash
docker compose run --rm simulator bash -c "cmake -S . -B build && cmake --build build -j && cd build && ctest"
```
