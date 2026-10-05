# Blackwell 機での job_server 構築手順

対象：RTX Pro 6000 Blackwell（96GB、1枚）、Ubuntu 26.04
モデル：`Qwen/Qwen3.8-27B`（revision `1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0`）

- `[Blackwell]` と書いた手順は Blackwell 機で、`[実験用PC]` は C++ をビルドする PC（Docker）で実行する
- 各手順の「確認」の出力は、次の手順に進む前に記録しておく（うまくいかなかったときに貼ってもらう）
- サーバーは研究室の共有物なので、`sudo` が必要な手順（§1.3、§9）は管理者（先生）に確認してから行う

---

## 0. モデルについて（調査結果）

| 項目 | 内容 |
|---|---|
| パラメータ | 27.8B（BF16、量子化なし）。重みは約 55.6GB（51.7GiB）、18ファイル |
| 構造 | 64層のうち48層が線形注意（Gated DeltaNet）、16層が通常の注意。MoE ではない（Dense） |
| 種類 | 画像・動画も扱える視覚言語モデル（`Qwen3_5ForConditionalGeneration`）。テキストだけ使うので `language_model_only=True` で画像エンコーダを読み込まない |
| 要件 | transformers 5.8.0 以上、vLLM 0.17.0 以上（vLLM の Qwen3.8 レシピ） |
| 思考モード | 既定で ON。生成プロンプトの末尾に `<think>\n` が付き、出力は思考 → `</think>` → 回答の順 |
| `reasoning_effort` | `xhigh`（既定）・`medium`・`low`。`xhigh` と `low` はシステムプロンプトの先頭に英語の指示文を足す。`medium` は何も足さない（§7） |
| 推奨サンプリング | 思考モードは temperature=1.0, top_p=0.95, top_k=20。本研究は再現性のため temperature=0 にする（設計書 §3.2）ので、`finish_reason == "length"` を監視する |
| MTP | 投機的デコーディング用の層があるが、使わない（`speculative_config` を指定しない） |

再現性に関わる未確認事項：

- vLLM の batch invariance（`VLLM_BATCH_INVARIANT=1`）の公式な対応表に、線形注意（Gated DeltaNet）を持つモデルは載っていない。§10 の検証4で、効いているかを実測する
- RTX Pro 6000（sm_120）は「compute capability 8.0 以上」という条件は満たすが、batch invariance の動作確認済み一覧には明記されていない

---

## 1. 事前確認 [Blackwell]

### 1.1 ログイン

踏み台を経由して入る（`<踏み台>` と `<blackwell>` は実際のホスト名に置き換える）：

```bash
ssh -J <ユーザー>@<踏み台> <ユーザー>@<blackwell>
```

### 1.2 環境の確認

次をまとめて実行し、出力を記録する：

```bash
lsb_release -ds; uname -r
nvidia-smi
nvidia-smi --query-gpu=name,compute_cap,driver_version,memory.total --format=csv
df -h ~ /tmp
free -g
nproc
which conda python3 git tmux; python3 --version; git --version
ls ~/miniconda3 2>/dev/null | head -3
ip -4 addr | grep inet
sudo -n ufw status 2>&1 | head -5
echo "HF_HOME=${HF_HOME:-未設定（~/.cache/huggingface）}"
```

確認：

- GPU 名に `RTX PRO 6000` を含み、`compute_cap` が `12.0`、`memory.total` が約 96GB
- ドライバのバージョン（CUDA 12.8 以上に対応するドライバが必要。§3 の確認で実際に動くかを見る）
- ホームの空き容量が **80GB 以上**（モデル 56GB ＋ Python 環境 約15GB ＋ ジョブの保存先）
- `tmux` があること（SSH が切れてもサーバーを動かし続けるため）

### 1.3 足りないものがあれば

- `tmux` や `git` がない → 管理者に `sudo apt install tmux git` を依頼
- ホームの容量が足りない → モデルの保存先（`HF_HOME`）を広いディスクに置く。場所は管理者に確認

---

## 2. リポジトリの取得 [Blackwell]

```bash
cd ~
git clone https://github.com/bp22029/SurveySimulator.git
cd SurveySimulator
git switch feature/job-server
git log --oneline -1
```

---

## 3. Python 環境 [Blackwell]

### 3.1 Miniconda（`~/miniconda3` がない場合だけ）

```bash
cd ~
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash Miniconda3-latest-Linux-x86_64.sh -b -p ~/miniconda3
~/miniconda3/bin/conda init bash
exec bash
```

### 3.2 環境の作成と vLLM の導入

Ubuntu 26.04 のシステムの Python は使わず、conda で Python 3.12 を入れる。
vLLM はバージョンを固定する（設計書 §6：同じ vLLM バージョンでしか再現性は保証されない）。

```bash
conda create -y -n job_server python=3.12
conda activate job_server
pip install "vllm==0.30.0"
pip install "transformers>=5.8.0"
pip check
mkdir -p ~/SurveySimulator/job_server/logs
pip freeze > ~/SurveySimulator/job_server/logs/pip_freeze_$(date +%Y%m%d).txt
```

`vllm==0.30.0` は 2026-09-22 時点の最新版。別のバージョンにする場合は、`start_server.sh` と一緒に2人で決めて記録する。

### 3.3 確認

```bash
python - <<'EOF'
import torch, vllm, transformers
print("torch", torch.__version__, "cuda", torch.version.cuda)
print("vllm", vllm.__version__, "transformers", transformers.__version__)
print("gpu", torch.cuda.get_device_name(0), torch.cuda.get_device_capability(0))
print("arch list", torch.cuda.get_arch_list())
x = torch.randn(1024, 1024, device="cuda", dtype=torch.bfloat16)
print("matmul ok", (x @ x).float().abs().sum().item() > 0)
from vllm import EngineArgs
print("language_model_only supported:", "language_model_only" in EngineArgs.__dataclass_fields__)
EOF
```

確認：

- `get_device_capability` が `(12, 0)`
- `torch` の `cuda` のバージョンを記録する（ドライバがその CUDA に対応している必要がある）、`arch list` に `sm_120`（または `sm_120a`）がある
- `matmul ok True`（ここで `no kernel image is available` が出たら、PyTorch が sm_120 に対応していない → §12）
- `transformers` が 5.8.0 以上
- `language_model_only supported: True`（False なら §12）

---

## 4. モデルのダウンロード [Blackwell]

revision を指定して取得する。約 56GB。

```bash
conda activate job_server
hf download Qwen/Qwen3.8-27B --revision 1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0
```

確認：

```bash
du -sh "${HF_HOME:-$HOME/.cache/huggingface}/hub/models--Qwen--Qwen3.8-27B"
ls "${HF_HOME:-$HOME/.cache/huggingface}/hub/models--Qwen--Qwen3.8-27B/snapshots/"
```

---

## 5. チャットテンプレートの確認 [Blackwell]

モデルに実際に渡される文字列を、reasoning_effort ごとに表示する（GPU は使わない）：

```bash
cd ~/SurveySimulator
python job_server/tools/show_chat_template.py \
    --system data/prompt_templates/forQwen/qwen_bfi2.txt \
    --user data/prompt_templates/user_prompt_template.txt
```

確認：プロンプトの各行に `\r` が混ざっていないこと（`cat -A data/prompt_templates/forQwen/qwen_bfi2.txt | head -3` で行末が `$` だけ。`^M$` なら改行が CRLF になっている）。

---

## 6. 起動スクリプトの設定 [Blackwell]

`job_server/start_server.sh` を確認する：

- `CONDA_SH`：§3.1 で入れた場所（既定 `~/miniconda3/etc/profile.d/conda.sh`）
- `CONDA_ENV="job_server"`
- `MODEL`・`REVISION`：設定済み
- `REASONING_EFFORT`：§7 で決める。**動作確認と検証の間は仮に `medium` にする**

設定を変えたら、2人の合意のうえで commit する。未コミットの変更があると、`GET /info` の
`git.dirty` が `true` になり、どの設定で動いたかを後から追えなくなる。

---

## 7. reasoning_effort を決める（研究上の判断）

| 値 | システムプロンプトへの追加 | 想定される影響 |
|---|---|---|
| `xhigh`（モデルの既定） | 先頭に "Reasoning effort is set to xhigh. Please think carefully..." | 思考が最も長い。`max_tokens=4096` で打ち切られる回答が増え、時間もかかる |
| `medium` | なし（昨年と同じく、こちらのシステムプロンプトだけ） | 中間 |
| `low` | 先頭に "Reasoning effort is set to low. Keep your thinking brief..." | 思考が最も短い |

- どれを選んでも温度0で再現性は保てるが、回答の分布と所要時間が変わる。最適化の途中では変えない
- 判断材料として、§10 の検証で使う20人分を各設定で1回ずつ流し、`n_length`（打ち切り件数）と所要時間を比べるとよい（§10.6）
- 決めたら `start_server.sh` に設定して commit し、設計書 §8 と論文の記載に残す

補足：システムプロンプト（`qwen_bfi2.txt`）は出力形式として `<think>…</think><answer>…</answer>` を指示しているが、
Qwen3.8 はモデル自体が `<think>` から書き始める。C++ 側は「最後の `</think>` の後の、最後の `<answer>`」を回答として
取り出すので、どちらの書き方でも読み取れる。プロンプトを変えるかどうかは別途判断する。

---

## 8. 起動 [Blackwell]

SSH が切れても止まらないように、tmux の中で起動する：

```bash
tmux new -s job_server
cd ~/SurveySimulator/job_server
bash start_server.sh
```

tmux から抜けるときは `Ctrl-b` → `d`。戻るときは `tmux attach -t job_server`。

確認（起動ログ）：

- `server info:` の JSON に、vLLM・torch・transformers のバージョン、GPU、`model`・`revision`、
  `engine.reasoning_effort`、`engine.chat_template_sample` が出ている
- `kv_cache_tokens` に数値が出ている（`null` なら vLLM の内部の場所が変わっただけで、動作には影響しない）
- 最後に `server is ready on 0.0.0.0:8000`
- 環境変数が足りないと `environment variables are not set by the start script` で止まる

別の端末から（Blackwell 上で）：

```bash
curl -s localhost:8000/queue
curl -s localhost:8000/info | python -m json.tool | head -40
```

止めるとき：tmux の中で `Ctrl-c`。実行中のジョブは、次の起動時に自動で再実行される。

---

## 9. ネットワーク [Blackwell・管理者]

研究室のネットワークからだけ 8000 番に入れるようにする（範囲は先生に確認：設計書 §8）：

```bash
sudo ufw allow from 192.168.130.0/24 to any port 8000 proto tcp
sudo ufw status
```

確認 [実験用PC]：

```bash
curl -s http://<BlackwellのIP>:8000/queue
```

実験用PC から直接届かない場合（踏み台が必要な場合）は、SSH のポート転送を使う：

```bash
ssh -N -J <ユーザー>@<踏み台> -L 8000:localhost:8000 <ユーザー>@<blackwell>
# 別の端末で
curl -s http://localhost:8000/queue
```

---

## 10. 再現性の検証（設計書 §5）

本番の前に行う。全員分（20,553件）を毎回流すと時間がかかるので、まず20人分（1,020件）で行う。

### 10.1 プロンプトの用意 [実験用PC]

```bash
cd SurveySimulator && git switch feature/job-server && git pull
docker compose run --rm simulator bash -c "cmake -S . -B build && cmake --build build -j && cd build && ctest"
docker compose run --rm simulator ./build/src/DumpPrompts config/batch_optimizer.example.json requests.json
```

- `ctest` がすべて通ること（特に `PortableRandomTest.FixedValuesForSeed42`）
- 合成人口の CSV（`data/2015_001_8+_47356.csv`、git 管理外）が実験用PC にあること
- `requests.json`（約42MB）は初期個性（seed 42）での全員分のプロンプト。commit しない

以降の `verify.py` は Python 3 があればどこでも動く（追加のパッケージ不要）。`SERVER` はサーバーの URL：

```bash
SERVER=http://<BlackwellのIP>:8000
V="python3 job_server/tools/verify.py"
```

### 10.2 検証1：同じ入力を2回

```bash
$V submit --server $SERVER --requests requests.json --client-id verify --sweep 1 --persons 20 --out r1.json
$V submit --server $SERVER --requests requests.json --client-id verify --sweep 2 --persons 20 --out r2.json
$V compare r1.json r2.json
```

合格：`different: 0`。`elapsed` と `n_length` も記録する。

### 10.3 検証2：他のクライアントのジョブを挟む

```bash
$V submit --server $SERVER --requests requests.json --client-id verify-b --sweep 1 --skip 20 --persons 10 --no-wait
$V submit --server $SERVER --requests requests.json --client-id verify --sweep 3 --persons 20 --out r3.json
$V submit --server $SERVER --requests requests.json --client-id verify-b --sweep 2 --skip 30 --persons 10 --no-wait
$V compare r1.json r3.json
```

合格：`different: 0`。

### 10.4 検証3：サーバーの再起動

§8 の方法でサーバーを止めて起動し直してから：

```bash
$V submit --server $SERVER --requests requests.json --client-id verify --sweep 4 --persons 20 --out r4.json
$V compare r1.json r4.json
```

合格：`different: 0`。一致しない場合は、起動ログの `engine.llm.enforce_eager` が `true` か確認する
（vLLM issue #58899：torch.compile の自動チューニングで再起動をまたぐと出力が変わる）。

### 10.5 検証4：batch invariance が効いているか

1人分（51件）だけのバッチで推論し、20人分のバッチの中での結果と比べる：

```bash
PID=$(python3 -c "import json;print(json.load(open('r1.json'))['results'][0]['id'].split('_')[0])")
$V submit --server $SERVER --requests requests.json --client-id verify --sweep 5 --person-ids $PID --out r5.json
$V compare r1.json r5.json
```

- 一致する → batch invariance が効いている。バッチの大きさや組み合わせに結果が依存しない
- 一致しない → 「同じバッチなら同じ結果」の保証しかない。一括方式では毎周 403人分を同じ並び（population の順）で
  1ジョブにするので運用上は成り立つが、1人だけを再推論して結果を比べる、といった使い方はできない。設計書 §5 に結果を記録する

### 10.6 検証5：速度（と reasoning_effort の比較）

- 全員分を1ジョブで流す（数時間かかる見込み。夜間に）：
  ```bash
  $V submit --server $SERVER --requests requests.json --client-id verify --sweep 10 --out r_full.json
  ```
- 1人ずつ流した場合の目安（5人分を個別のジョブで流し、`s/person` を403倍する）：
  ```bash
  for i in 0 1 2 3 4; do
    $V submit --server $SERVER --requests requests.json --client-id verify --sweep $((20+i)) --skip $i --persons 1 --out r_single_$i.json
  done
  ```
- batch invariance の有無による速度差：`start_server.sh` の `VLLM_BATCH_INVARIANT=0` で起動し直し、
  `--client-id verify-bi0` として 10.2 と同じ20人分を流して `elapsed` を比べる（比べ終わったら 1 に戻す）
- reasoning_effort の比較（§7）：`REASONING_EFFORT` を変えて起動し直し、`--client-id verify-xhigh` などとして
  20人分を流し、`n_length` と `elapsed` を比べる

検証の記録には、各回の `GET /info`（`jobs_data/server_info_*.json`）を添える。

---

## 11. 本番の前に

- [ ] `start_server.sh` の `REASONING_EFFORT` を決めて commit した。`GET /info` の `git.dirty` が `false`
- [ ] §10 の検証1〜3が合格。検証4・5の結果を設計書 §5 に記録した
- [ ] SA の初期温度・終了温度・周回数を決めた（設計書 §8）
- [ ] 2人それぞれの設定ファイル（`config/batch_optimizer.example.json` をコピー）で、`client_id` と `run_dir` が別
- [ ] `real_ratios.csv` と合成人口の CSV を2人で同じものにした（改行は LF。CRLF だと BatchOptimizer が止まる）
- [ ] 実験用PC で `ctest` がすべて通る

本番の実行 [実験用PC]：

```bash
docker compose run --rm simulator ./build/src/BatchOptimizer my_config.json
```

---

## 12. うまくいかないとき

| 症状 | 対処 |
|---|---|
| `no kernel image is available for execution on the device` | PyTorch・vLLM のビルドが sm_120 に対応していない。`pip show torch` の版と §1.2 のドライバを記録して相談する |
| `model type qwen3_5 ... not recognized` | transformers が古い。`pip install "transformers>=5.8.0"` |
| `unexpected keyword argument 'language_model_only'` | その vLLM では使えない。`engine.py` の `language_model_only=True` の行を外す（画像エンコーダの分だけメモリを使う） |
| 起動時に CUDA out of memory | `GPU_MEMORY_UTILIZATION` は上げすぎない（他の利用者がいないか `nvidia-smi` で確認）。`MAX_MODEL_LEN` を下げる前に相談する |
| `VLLM_BATCH_INVARIANT=1` で起動時にエラー | このモデルの線形注意の部分が batch invariance に対応していない可能性。エラー全文を記録し、`VLLM_BATCH_INVARIANT=0` で検証1〜4を行う（設計書 §5-4 の「同じバッチなら同じ結果」での運用） |
| `n_length` が多い | `MAX_TOKENS`（4096）で思考が打ち切られている。reasoning_effort を下げるか `MAX_TOKENS` を上げる（2人で合意のうえで） |
| `HTTP 409` | 同じ `client_id` と `sweep` で別の内容をすでに投げている。検証では `sweep` を変える |
