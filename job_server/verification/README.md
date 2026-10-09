# 再現性の検証の記録

[SETUP_BLACKWELL.md §10](../SETUP_BLACKWELL.md) の検証の結果。各回の結果（`r*.json.gz`）は `verify.py submit` の出力を gzip したもので、
`verify.py compare` にそのまま渡せる。

## 2026-10-08：reasoning_effort=medium（`2026-10-08_medium/`）

### 条件

| 項目 | 値 |
|---|---|
| サーバー | job_server commit `3ca8f02`（一式で送付、`git.dirty: false`） |
| 環境 | vLLM 0.30.0、torch 2.13.0+cu132、transformers 5.18.0、RTX PRO 6000 Blackwell Max-Q（ドライバ 615.71.09） |
| モデル | `Qwen/Qwen3.8-27B` @ `1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0` |
| 推論設定 | `REASONING_EFFORT=medium`（起動時に指定）、temperature 0、`MAX_TOKENS` 8192（当時はサーバー全体の設定。現在はジョブごとに `--enable-thinking true --reasoning-effort medium --max-tokens 8192` と指定するのと同じ）、`MAX_MODEL_LEN` 16384、`GPU_MEMORY_UTILIZATION` 0.9、`enforce_eager: true`、`VLLM_BATCH_INVARIANT=0` |
| スケジューラ | `max_num_seqs` 1024、`max_num_batched_tokens` 16384、`num_gpu_blocks` 648（再起動の前後で同じ） |
| 入力 | `config/batch_optimizer.example.json`（`forQwen/qwen_bfi2.txt`、初期個性 seed 42）の `DumpPrompts` 出力の先頭20人（person_id 114467207〜114467226、1,020件） |

サーバー情報（`GET /info`）は `server_info_before_restart.json`・`server_info_after_restart.json`。起動時刻以外は同じ。

### 結果

| 検証 | 内容 | client_id / sweep | 結果 |
|---|---|---|---|
| 1 | 同じ20人を2回 | verify / 1, 2 | 1,020件すべて一致 **合格** |
| 2 | verify-b（10人ずつ）のジョブを前後に挟む | verify / 3 | r1 と1,020件すべて一致 **合格** |
| 3 | サーバーを再起動してから | verify / 4 | r1 と1,020件すべて一致 **合格** |
| 4 | 1人（114467207、51件）だけのジョブ | verify / 5 | r1 と比べて全文の違い 49/51件、**最終回答の違い 9/51件**（どれも隣の番号へのずれ） |

- 同じバッチを同じ設定で流せば、繰り返し・他のクライアントとの交互利用・再起動をまたいでも、出力は完全に同じ
- batch invariance が使えないため、ある人の回答は同じジョブの他の人の内容に左右される（検証4）。思考の書き出し（数十文字目）から
  分かれる件もある。1人だけを再推論して以前の結果と比べる使い方はできない
- 所要時間：20人分のジョブは 724.8〜728.5秒（4回、約36秒/人）、1人分のジョブは 64.8秒
- 出力トークン：平均 763、p50 762、p99 1187、最大 1392。`finish_reason` はすべて `stop`（打ち切り 0件）
- 全員分（403人）を1ジョブで流す検証5は未実施

保存しているのは r1（基準）と r5（検証4）。r2〜r4 は r1 と同一なので保存していない。

### 同じ環境かを確かめる

サーバーや設定を変えていなければ、同じ20人を流すと r1 と完全に一致するはず。研究用PC から（`SERVER`・`requests.json` は §10.1）：

```bash
V="python3 job_server/tools/verify.py"
$V submit --server $SERVER --requests requests.json --enable-thinking true --reasoning-effort medium --max-tokens 8192 \
    --client-id verify --sweep <未使用の番号> --persons 20 --out r_check.json
$V compare job_server/verification/2026-10-08_medium/r1.json.gz r_check.json
```

`requests.json` は `config/batch_optimizer.example.json` から作ったもの（プロンプトを変えると一致しない）。
`client_id` と `sweep` の組はジョブごとに一意なので、すでに使った sweep（verify は 1〜5）は避ける。
r1・r5 の保存ファイルには推論条件が入っていないので、`compare` では `enable_thinking=None` と表示される。

推論条件をジョブごとに受け取るようにした変更（2026-10-09）で、サーバーのコードが変わった。推論の呼び出し方は同じだが、
新しい一式を送ったら上のコマンドで r1 と一致することを確かめてから本番に進む。

## 2026-10-09：推論条件をジョブごとに指定する変更の後（`5b37582`）

job_server を commit `5b37582` の一式に入れ替え（`git.dirty: false`、`scheduler`・`cache.num_gpu_blocks` 648 は 10/8 と同じ）、
研究室PC から同じ20人を `--enable-thinking true --reasoning-effort medium --max-tokens 8192 --client-id verify2 --sweep 1` で流した（job 16）。

- r1 と比べて 1,020件すべて一致（`identical: 1020  different: 0  different final answer: 0`）**合格**
- 所要時間 724.0秒（36.2秒/人）。出力トークン：平均 763、p50 762、p99 1187、最大 1392。打ち切り 0件
