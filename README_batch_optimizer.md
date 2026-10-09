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
cp .env.example .env                                    # 初回だけ。JOB_SERVER_URL を Blackwell の URL にする
cp config/batch_optimizer.example.json my_config.json   # 編集する
./build/src/BatchOptimizer my_config.json
```

- サーバーの URL は設定ファイルには書かない。環境変数 `JOB_SERVER_URL`、なければリポジトリのルートの `.env`
  （`JOB_SERVER_URL=http://192.168.130.XXX:8000`）から読む。`.env` は commit しない（`.gitignore` 済み）。
  起動時に、どこから読んだ URL かを表示する。URL は再現性の照合に使わない（サーバーの同一性は `/info` で照合する）
- 相対パスは、実行したときのカレントディレクトリ（リポジトリのルートを想定）から解決される
- `client_id` は人ごとに変える。`(client_id, sweep)` はサーバーに残り続けるので、条件を変えて最初からやり直すときは
  別の名前にする（例：`bp22029-q38-medium-r1`。後輩は自分のアカウント名で始める）
- `enable_thinking`・`reasoning_effort`（`xhigh`・`medium`・`low`）・`max_tokens` は推論条件で、各自の研究で決めて書く。
  思考なし（`"enable_thinking": false`）のときは `reasoning_effort` を書かない。サーバーはジョブごとにこの条件で推論するので、
  2人が別の値を使ってもよい。最適化した個性はその条件でのものなので、個性を使う実験でも思考の有無と effort はできるだけ揃える
  （job_server/SETUP_BLACKWELL.md §7）
- 途中で止まっても、同じ設定ファイルで起動し直せばチェックポイントから再開する。
  サーバーに同じ周のジョブが残っていれば、投げ直さずにその結果を使う
- 途中で設定（推論条件、温度、周回数、seed、プロンプトなど）を変えて再開しようとすると止まる。
  サーバーのモデル・revision・vLLM などのバージョンが変わっていた場合も止まる

## 温度を決める：CalibrateTemperature

SA の初期温度・終了温度は、昨年度と同じく山田らの方法（悪化した変更案を受け入れる割合 μAG比 が
50％ になる温度を初期温度、0.2％ になる温度を終了温度）で決める。ΔE の大きさはモデルや推論条件で
変わるので、本番と同じ設定ファイルで測ってから決める。

```bash
./build/src/CalibrateTemperature my_config.json 3 runs/bp22029_calibration
```

- 初期状態（本番の sweep 0 と同じ個性）を推論し、全員に変更案を1つずつ作って推論し直し、
  「その人の回答だけを入れ替えたときの ΔTAE」を1人ずつ求める。これを指定した回数（例：3回）繰り返す。
  採否は判定しないので、途中の温度に左右されない「出発点での ΔE の分布」になる
- 1回 ＝ 全員分の推論1ジョブ。初期状態と合わせて (回数＋1) ジョブを投げる
- 初期状態と1回目は、本番の sweep 0・sweep 1 とまったく同じ入力なので、本番と同じ `client_id` で投げる。
  あとで同じ設定ファイルで `BatchOptimizer` を始めると、サーバーはその結果を返し、推論し直さない。
  2回目以降は `<client_id>-calib` で投げる
- 測ったあとで推論条件・seed・プロンプト・変異の幅を変えると、本番の sweep 0 が合わなくなる（HTTP 409）。
  その場合は本番の `client_id` を変える
- 途中で止まっても、同じ引数で起動し直せば、終わったジョブはサーバーの結果を使う

出力（`out_dir`。本番の `run_dir` とは別にする）：

| ファイル | 内容 |
|---|---|
| `calibration_deltas.csv` | 1人ずつの ΔTAE（round, client_id, sweep, job_id, person_id, delta, skipped） |
| `calibration_summary.json` | 初期 TAE、回ごとと全体の件数（悪化・改善・変化なし）、悪化の平均 ΔĒ、2つの方法による温度 |
| `initial_population.csv` | 初期状態の個性と回答（sweep 0 の population と同じ） |

温度は2つの方法で出す（終了時に表示され、`calibration_summary.json` の `temperatures` に入る）：

- `mean_delta_method`（昨年度と同じ）：T₀ = ΔĒ / ln 2、T_f = ΔĒ / ln 500。昨年度は Qwen3-14B で ΔĒ ≈ 0.0141 → 0.02034 / 0.002269
- `distribution_method`：測った ΔE の分布について、受け入れる割合の期待値（ΔE ごとの exp(−ΔE/T) の平均）が
  ちょうど 50％・0.2％ になる温度。ΔE のばらつきが大きいと、平均だけで決めた温度では終了時の割合が 0.2％ より高くなる
  （昨年度の本番では最後の周で 2〜4％ だった）

どちらの方法でも、設定ファイルの周回数で冷却率 α = (T_f/T₀)^(1/(周回数×人数)) を表示する。

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
