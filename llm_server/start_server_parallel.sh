#!/bin/bash
set -e
# Condaの読み込み (ご自身の環境に合わせてください)
source /home/bp22029/miniconda3/etc/profile.d/conda.sh
conda activate vllm_lab

# 既存のプロセスがあればキルする（二重起動防止のため入れると便利です）
pkill -f "python server.py" || true

echo "Starting vLLM Servers..."

trap "echo 'Stopping servers...'; pkill -f 'python server.py'; exit" INT TERM

# --- 1台目: GPU 0, Port 8000 ---
# CUDA_VISIBLE_DEVICES環境変数をコマンドの頭につけて渡します
# & をつけることでバックグラウンド実行にします
CUDA_VISIBLE_DEVICES=0 python server.py --port 8000 > server_0.log 2>&1 &

# --- 2台目: GPU 1, Port 8001 ---
CUDA_VISIBLE_DEVICES=1 python server.py --port 8001 > server_1.log 2>&1 &

echo "Servers actvated on ports 8000 and 8001."

echo "Servers activated. Press Ctrl+C to stop."

# スクリプトが終了しないように待機
wait