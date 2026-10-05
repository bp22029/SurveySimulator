#!/bin/bash
# 個性割り当て用LLMサーバーの起動スクリプト（Blackwell 機で実行する）
#
# このファイルの設定値を変えるときは、2人の合意のうえで commit してから起動すること。
# 起動ログとサーバー情報に、このリポジトリの commit hash が記録される（設計書 §6）。
set -euo pipefail

# --- 環境に合わせる -----------------------------------------------------
CONDA_SH="${HOME}/miniconda3/etc/profile.d/conda.sh"
CONDA_ENV="vllm_lab"
PORT=8000

# --- モデル（未決定：設計書 §8） ----------------------------------------
MODEL=""      # 例: Qwen/xxx-27B
REVISION=""   # Hugging Face の commit hash（40桁）

# --- 推論設定（見直し対象） --------------------------------------------
MAX_MODEL_LEN=8192
GPU_MEMORY_UTILIZATION=0.8
MAX_TOKENS=4096

# --- 再現性のための環境変数（Python コード内では設定しない） -------------
export PYTHONHASHSEED=42
export VLLM_ENABLE_V1_MULTIPROCESSING=0
export VLLM_BATCH_INVARIANT=1
export CUBLAS_WORKSPACE_CONFIG=:4096:8

if [[ -z "${MODEL}" || -z "${REVISION}" ]]; then
    echo "MODEL と REVISION を start_server.sh に設定してください" >&2
    exit 1
fi

cd "$(dirname "$0")"
source "${CONDA_SH}"
conda activate "${CONDA_ENV}"

mkdir -p logs
python server.py \
    --port "${PORT}" \
    --data-dir jobs_data \
    --model "${MODEL}" \
    --revision "${REVISION}" \
    --max-model-len "${MAX_MODEL_LEN}" \
    --gpu-memory-utilization "${GPU_MEMORY_UTILIZATION}" \
    --max-tokens "${MAX_TOKENS}" \
    2>&1 | tee "logs/server_$(date +%Y%m%d_%H%M%S).log"
