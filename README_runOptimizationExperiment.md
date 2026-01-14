# SurveySimulator: `runOptimizationExperiment` 実験手順書

このドキュメントは、このリポジトリ一式を受け取った人が **`src/main.cpp` の `runOptimizationExperiment`** を実行して最適化実験（SA: Simulated Annealing）を回せるようにするための手順書です。

> 重要: このプロジェクトは C++ 側が **ファイル監視型ブリッジ** で Python(vLLM) 側に推論を依頼します。
> そのため、**`resident_worker.py` と `src/llm_offline.cpp`（加えて `src/experiment_runner.cpp`）の `BRIDGE_DIR/REQUEST_FILE/RESPONSE_FILE` を自分の環境に合わせて変更する必要があります**。

---

## 1. 全体像（何が動く？）

- C++（`SurveySimulator`）
  - 人口データと質問データを読み込み
  - 既存のベースライン回答（`merged_population_..._initial.csv`）をロード
  - SA（焼きなまし）で **特定エージェントの性格パラメータを少しだけ変異**
  - 変異した 1 人分の回答だけを vLLM に推論依頼（`getResponsesForPerson`）
  - MAE が改善すれば採用、悪化しても確率で採用
  - 定期的にチェックポイントCSVを書き出し、最後に結果CSVを出力

- Python（`resident_worker.py`）
  - `bridge_request.json` を監視
  - C++ が書いた system/user prompt を読んで vLLM で推論
  - `bridge_response.json` に JSON 配列として結果を書き戻す

---

## 2. 事前準備

### 2.1 必要なもの

- WSL2：フォルダでのやり取りがあるので、vLLMがある環境上(WSL2)にCLionのプロジェクトがあることを想定しています
- C++17 が使えるコンパイラ + CMake（このリポジトリは CMake でビルドします）
- Python（vLLM が動く環境）3.10.19
- vLLM バージョン 0.11.1
- GPU + vLLM が動作する CUDA 環境（`resident_worker.py` は vLLM を使います）

> 補足: CMake は `FetchContent` で `cpp-httplib`, `nlohmann/json`, `googletest` を取得します。ネットワークに出られる環境で最初の configure/build をしてください。

### 2.2 ブリッジ用ディレクトリを作る

C++ と Python がファイルをやりとりするディレクトリを作ります。

- 例（既定値）: `/home/bp22029/vllm_bridge`

このディレクトリに以下2ファイルが作られます。

- `bridge_request.json`（C++ → Python）
- `bridge_response.json`（Python → C++）

---

## 3. 必須: ブリッジパス設定（環境依存）

あなたの環境に合わせて、**必ず** 次のファイル内の `BRIDGE_DIR/REQUEST_FILE/RESPONSE_FILE` を更新してください。

### 3.1 `resident_worker.py`

該当箇所（先頭付近）:

- `BRIDGE_DIR = "/home/bp22029/vllm_bridge"`
- `REQUEST_FILE = os.path.join(BRIDGE_DIR, "bridge_request.json")`
- `RESPONSE_FILE = os.path.join(BRIDGE_DIR, "bridge_response.json")`

ここを **あなたのブリッジディレクトリ** に変更します。

### 3.2 `src/llm_offline.cpp`

該当箇所（上部）:

- `const std::string BRIDGE_DIR = "/home/bp22029/vllm_bridge";`
- `const std::string REQUEST_FILE = BRIDGE_DIR + "/bridge_request.json";`
- `const std::string RESPONSE_FILE = BRIDGE_DIR + "/bridge_response.json";`

Python側と **完全に同じパス** になるように揃えてください。

### 3.3 （強く推奨）`src/experiment_runner.cpp` も揃える

`runOptimizationExperiment` の初期化処理でも同じブリッジファイルを消し込みしています。

該当箇所（ファイル冒頭付近）:

- `const std::string BRIDGE_DIR = ...`
- `const std::string REQUEST_FILE = ...`
- `const std::string RESPONSE_FILE = ...`

ここも **同じ値** にしてください。

---

## 4. データ/テンプレートの配置（相対パスに注意）

`src/main.cpp` は以下の相対パスでファイルを読みに行きます。

> ポイント: 実行時のカレントディレクトリによって `../../data/...` が指す場所が変わります。
> **基本はビルドディレクトリ（例: `cmake-build-.../src`）から実行する** ことを想定しています。

### 4.1 参照される主なファイル

- 合成人口: `../../data/2015_001_8+_47356.csv`
- テスト人口: `../../data/verification_population.csv`
- 質問: `../../data/ssm2015.csv`
- ユーザープロンプト: `../../data/prompt_templates/user_prompt_template.txt`
- システムプロンプト（例）:
  - `../../data/prompt_templates/forQwen/qwen_bfi2.txt`（`main.cpp` の `system_prompt_templates_forQwen` で使用）
- 正解比率（MAE計算用）: `../../data/real_ratios.csv`

### 4.2 ベースライン（initial）CSV

現在の `src/experiment_runner.cpp` は、vLLM で最初からベースラインを生成する代わりに、**既存の initial CSV を読み込んで初期状態を復元** する実装になっています。

該当箇所:

- `std::string initial_csv_path = "../../data/merged_population_20251231_151241_initial.csv";`

あなたの環境にこのファイルが無い場合は、次のいずれかを選んでください。

1) **このパスを、手元にある `merged_population_..._initial.csv` に変更する**（推奨）
2) ベースライン生成を有効化する（`experiment_runner.cpp` 内のコメントアウトを外して `runSurveySimulation_Resident(...)` から作る）

---

## 5. Python（vLLMワーカー）を起動する

別ターミナルで `resident_worker.py` を起動し、`bridge_request.json` を監視状態にします。

- 実行すると `REQUEST_FILE` を監視し続けます
- C++ が `bridge_request.json` を置くと、まとめて推論し、`bridge_response.json` を生成します

> 注意: `resident_worker.py` の `CUDA_VISIBLE_DEVICES` やモデル名（例: `Qwen/Qwen3-14B`）は環境に応じて調整してください。

---

## 6. C++ をビルドする

このプロジェクトは CMake 構成です。

- ルート: `SurveySimulator/CMakeLists.txt`
- 実行ファイル: `SurveySimulator`（CMakeターゲット名）

またテスト実行ファイルとして `run_tests` も定義されています。

---

## 7. 実験（`runOptimizationExperiment`）を実行する

`src/main.cpp` 末尾で `runOptimizationExperiment(...)` が呼ばれています。

実行時に行われること（概要）:

- `ExperimentConfig`（温度、反復回数、mutation幅、正解データパス、ログ出力先）を読み
- `runOptimizationExperiment(population, questions, system_prompt_templates_forQwen, user_prompt_template, config)` を開始
- `real_ratios.csv` を読み込み
- initial CSV から `population` と `responseManager` を復元
- ループごとに1人を微小変異 → vLLM（Python）へ1人分推論 → MAE 改善すれば採用

---

## 8. 生成物（出力）

`runOptimizationExperiment` が作る主な出力:

- ログCSV（`config.log_file_path` を元に、タイムスタンプ付きファイル名で保存）
  - 例: `../../log/sa_optimization_YYYYmmdd_HHMMSS.csv`
  - 付随詳細ログ: `...csv.detail.txt`
- 最終結果
  - `../../results/individual_responses_<timestamp>_final.csv`
  - `../../data/merged_population_<timestamp>_final.csv`
- チェックポイント
  - `../../data/checkpoint_latest.csv`（採用されたときに更新）

---

## 9. よくあるトラブルと対処

### 9.1 C++ が永遠に待つ（`bridge_response.json` が来ない）

- Pythonワーカーが起動しているか確認
- `BRIDGE_DIR/REQUEST_FILE/RESPONSE_FILE` が **C++ と Python で一致**しているか確認
- `BRIDGE_DIR` のディレクトリが存在するか確認
- 書き込み権限があるか確認

### 9.2 `initial_csv_path` が存在しない

- `src/experiment_runner.cpp` の `initial_csv_path` を手元のファイル名に変更
- もしくはベースライン生成（コメントアウトを外す）に切り替え

### 9.3 相対パスでファイルを読めない

- 実行時のカレントディレクトリが想定と違う可能性があります。
- `../../data/...` が解決できる場所（一般にビルドディレクトリ配下）から実行してください。

---

## 10. 変更が必要になりがちな箇所まとめ

環境に依存し、受け取った人が最初に直すことが多いポイントです。

- **必須**: `resident_worker.py` の `BRIDGE_DIR/REQUEST_FILE/RESPONSE_FILE`
- **必須**: `src/llm_offline.cpp` の `BRIDGE_DIR/REQUEST_FILE/RESPONSE_FILE`
- **推奨**: `src/experiment_runner.cpp` の `BRIDGE_DIR/REQUEST_FILE/RESPONSE_FILE`
- **要確認**: `src/experiment_runner.cpp` の `initial_csv_path`
- 任意: `resident_worker.py` の `CUDA_VISIBLE_DEVICES` と `model`（VRAMに合わせる）

