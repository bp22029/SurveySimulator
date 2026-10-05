# job_server：個性割り当て用バッチ推論サーバー

vLLM のオフライン `LLM` クラスを、ジョブキュー付きの FastAPI でサーバー化したもの。
昨年度（Qwen3-14B、GPU 2枚）のサーバーは `llm_server/` に当時のまま残してある。

複数のクライアント（2人）が同じサーバーを使っても、それぞれの結果が変わらないようにする。

- GPU で同時に実行するのは常に 1 ジョブだけ。1 ジョブにつき 1 回 `llm.generate()` を呼ぶ
- ジョブは到着順（job_id 順）に実行する
- サンプリング設定（temperature など）はサーバー側で固定し、クライアントからは指定できない
- `vllm serve`、AsyncLLMEngine、複数ジョブの同時実行、prefix caching の有効化には変えないこと

## ファイル

| ファイル | 役割 |
|---|---|
| `start_server.sh` | 起動スクリプト。モデル・revision・環境変数はここで設定する |
| `server.py` | エントリポイント。環境変数の確認、モデルの読み込み、起動情報の記録 |
| `app.py` | API |
| `worker.py` | 推論を行う唯一のスレッド |
| `job_store.py` | ジョブの保存（SQLite + gzip ファイル）。再起動しても消えない |
| `engine.py` | vLLM の設定と呼び出し |
| `environment.py` | 起動時の環境確認と、バージョン等の収集 |

## 起動（Blackwell 機）

環境構築から検証までの手順は [SETUP_BLACKWELL.md](SETUP_BLACKWELL.md)。

1. `start_server.sh` の `MODEL`・`REVISION`（Hugging Face の commit hash 40桁）・`REASONING_EFFORT` を設定し、commit する
2. `bash start_server.sh`

起動時に、vLLM・torch・transformers のバージョン、GPU、ドライバ、モデルと revision、
このリポジトリの commit hash（未コミットの変更の有無も）、KV キャッシュ容量、チャットテンプレートの
適用例をログと `~/llmsrv/data/jobs/server_info_*.json` に記録する。同じ内容を `GET /info` で取得できる。

再現性のための環境変数（`PYTHONHASHSEED` など）が設定されていない場合は起動しない。

## API

| メソッド | パス | 説明 |
|---|---|---|
| POST | `/jobs` | ジョブを投入する。すぐに `{"job_id": n, "created": bool}` を返す |
| GET | `/jobs/{job_id}` | 状態を返す。`done` のときだけ `results` を含む |
| GET | `/jobs?client_id=...&sweep=...` | 該当するジョブの一覧（クライアント再起動時の復旧用） |
| GET | `/queue` | 実行中と待機中のジョブ |
| GET | `/info` | 起動時に記録したサーバー情報 |

POST /jobs の本文：

```json
{
  "client_id": "bp22029",
  "sweep": 1,
  "requests": [
    {"id": "123_dq2_1", "system_prompt": "...", "user_prompt": "..."}
  ]
}
```

- `(client_id, sweep)` はジョブごとに一意。同じ組み合わせで同じ `requests` を再度 POST すると、
  新しいジョブは作らず既存の job_id を返す（`created: false`）。POST をリトライしても二重に登録されない
- 同じ組み合わせで中身の違う `requests` を POST すると 409
- 既存のジョブが `failed` なら、同じ内容の再 POST で `queued` に戻して再実行する（job_id は変わらない）
- 未知の項目（`temperature` など）や、重複した `id` を含む本文は 422
- 検証で同じ入力を2回推論したいときは、`sweep` か `client_id` を変えて投入する

GET /jobs/{job_id}（`done` のとき）：

```json
{
  "job_id": 1, "client_id": "bp22029", "sweep": 1, "status": "done",
  "n_requests": 20553, "n_length": 3,
  "results": [{"id": "123_dq2_1", "response": "...", "finish_reason": "stop"}]
}
```

結果は数百 MB になりうるので、`Accept-Encoding: gzip` を付けて取得すること（libcurl なら
`CURLOPT_ACCEPT_ENCODING` に `""`、curl なら `--compressed`）。保存済みの gzip をそのまま返す。
`attempts` や各時刻などのメタデータは `GET /jobs?client_id=...` で取得する。

## サーバーの再起動

ジョブの入力と結果は `~/llmsrv/data/jobs/`（場所は `env.sh` と `start_server.sh` で決まる）に保存される。実行中にサーバーが止まったジョブは、
次の起動時に `queued` に戻り、job_id 順に再実行される。

## テスト（GPU 不要）

```bash
pip install fastapi uvicorn httpx pytest
pytest job_server/tests
```

vLLM の代わりに入力を返すだけのエンジンで起動して、API を手元で試すこともできる：

```bash
python job_server/server.py --echo-engine --host 127.0.0.1 --port 8765 --data-dir /tmp/jobs_data
```
