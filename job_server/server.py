"""個性割り当て用のバッチ推論サーバー（設計書: 個性割り当て用LLMサーバー 設計決定事項）。

vLLM のオフライン LLM クラスを、ジョブキュー付きの FastAPI でサーバー化する。
GPU で同時に実行するのは常に 1 ジョブだけで、1 ジョブにつき 1 回 generate() を呼ぶ。
環境変数は起動スクリプト（start_server.sh）で設定すること。
"""
import argparse
import json
import logging
import re
import sys
import time
from pathlib import Path

import uvicorn

from app import create_app
from engine import EchoEngine, VllmEngine
from environment import check_env, collect_server_info
from job_store import JobStore
from worker import Worker

log = logging.getLogger("job_server")


def parse_args():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=8000)
    p.add_argument("--data-dir", type=Path, default=Path("jobs_data"),
                   help="ジョブの入力と結果を保存するディレクトリ")
    p.add_argument("--model", help="Hugging Face のモデル名")
    p.add_argument("--revision", help="Hugging Face の commit hash（40桁）")
    p.add_argument("--max-model-len", type=int, default=8192)
    p.add_argument("--gpu-memory-utilization", type=float, default=0.8)
    p.add_argument("--max-tokens", type=int, default=4096)
    p.add_argument("--reasoning-effort", choices=["xhigh", "medium", "low"],
                   help="思考の深さ（Qwen3.8 のチャットテンプレートの引数）。省略不可")
    p.add_argument("--echo-engine", action="store_true",
                   help="vLLM を使わず入力を返すだけのエンジンで起動する（GPU のない環境での動作確認用）")
    return p.parse_args()


def main():
    logging.basicConfig(
        level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s"
    )
    args = parse_args()

    if args.echo_engine:
        engine = EchoEngine()
    else:
        problems = check_env()
        if problems:
            sys.exit("environment variables are not set by the start script:\n  " + "\n  ".join(problems))
        if not args.model or not args.revision or not args.reasoning_effort:
            sys.exit("--model, --revision and --reasoning-effort are required")
        if not re.fullmatch(r"[0-9a-f]{40}", args.revision):
            sys.exit(f"--revision must be a 40-character commit hash: {args.revision!r}")

    store = JobStore(args.data_dir)
    recovered = store.recover()
    if recovered:
        log.info("requeued %d job(s) that were running when the server stopped", recovered)

    if not args.echo_engine:
        log.info("loading model %s@%s ...", args.model, args.revision)
        engine = VllmEngine(
            model=args.model,
            revision=args.revision,
            max_model_len=args.max_model_len,
            gpu_memory_utilization=args.gpu_memory_utilization,
            max_tokens=args.max_tokens,
            reasoning_effort=args.reasoning_effort,
        )

    info_json = json.dumps(collect_server_info(engine, args), ensure_ascii=False, indent=2, default=str)
    info = json.loads(info_json)
    log.info("server info:\n%s", info_json)
    info_path = args.data_dir / f"server_info_{time.strftime('%Y%m%d_%H%M%S')}.json"
    info_path.write_text(info_json, encoding="utf-8")

    worker = Worker(store, engine)
    worker.start()

    app = create_app(store, worker.notify, info)
    log.info("server is ready on %s:%d", args.host, args.port)
    uvicorn.run(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
