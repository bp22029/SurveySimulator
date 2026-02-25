#!/bin/bash
set -e

source /home/bp22029/miniconda3/etc/profile.d/conda.sh
conda activate vllm_lab

# クリーンアップ（二重起動防止）
pkill -f "python server.py" || true

echo "Starting Single vLLM Server..."

trap "echo 'Stopping server...'; pkill -f 'python server.py'; exit" INT TERM

# サーバー起動 (GPU 1を使用)
CUDA_VISIBLE_DEVICES=1 python server.py --port 8000 > server_single.log 2>&1 &

echo "Server activated on port 8000 (GPU 1)."

# プロセス終了待ち
wait