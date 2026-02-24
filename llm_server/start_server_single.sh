#!/bin/bash

# エラーがあれば停止
set -e

# 1. Condaの初期化 (パスは環境に合わせて調整。WSL2/Ubuntu標準的なパスです)
#    通常 ~/.bashrc に書かれている内容ですが、スクリプト内で明示的に読み込みます
source /home/bp22029/miniconda3/etc/profile.d/conda.sh
# または source ~/miniconda3/etc/profile.d/conda.sh

# 2. 仮想環境のアクティベート (例: vllm_env という名前の場合)
conda activate vllm_lab

# 3. サーバーの起動
#    --reload オプションをつけておくと、Pythonコード変更時に自動再起動して便利です
#    hostは WSL2/Ubuntu 内での通信なら 127.0.0.1 でOK
echo "Starting vLLM FastAPI Server..."
CUDA_VISIBLE_DEVICES=1 python server.py --port 8000 > server_0.log 2>&1 &
# または uvicorn server:app --host 127.0.0.1 --port 8000 --reload