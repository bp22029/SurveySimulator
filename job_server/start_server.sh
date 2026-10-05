#!/bin/bash
# 個性割り当て用LLMサーバーの起動スクリプト（Blackwell 機で実行する）
#
# このファイルの設定値を変えるときは、2人の合意のうえで commit してから起動すること。
# 起動ログとサーバー情報に、このリポジトリの commit hash が記録される（設計書 §6）。
set -euo pipefail

# --- 環境に合わせる -----------------------------------------------------
CONDA_SH="${HOME}/miniconda3/etc/profile.d/conda.sh"
CONDA_ENV="job_server"
PORT=8000
# ジョブの入力と結果・ログ・モデルのキャッシュの置き場所。コード（このディレクトリ）とは分ける
DATA_ROOT="${HOME}/job_server_data"

# --- モデル -------------------------------------------------------------
MODEL="Qwen/Qwen3.8-27B"
REVISION="1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0"   # 2026-08-14 更新の版
# 思考の深さ: xhigh（モデルの既定）/ medium / low。研究として決めて設定する（SETUP_BLACKWELL.md §7）
# 検証のときだけ `REASONING_EFFORT=medium bash start_server.sh` のように起動時に上書きできる
REASONING_EFFORT="${REASONING_EFFORT:-}"

# --- 推論設定（見直し対象） --------------------------------------------
MAX_MODEL_LEN=8192
GPU_MEMORY_UTILIZATION=0.8
MAX_TOKENS=4096

# --- 再現性のための環境変数（Python コード内では設定しない） -------------
export PYTHONHASHSEED=42
export VLLM_ENABLE_V1_MULTIPROCESSING=0
# 速度の比較（SETUP_BLACKWELL.md §10.6）のときだけ `VLLM_BATCH_INVARIANT=0 bash start_server.sh` で上書きできる
export VLLM_BATCH_INVARIANT="${VLLM_BATCH_INVARIANT:-1}"
export CUBLAS_WORKSPACE_CONFIG=:4096:8

if [[ -z "${MODEL}" || -z "${REVISION}" || -z "${REASONING_EFFORT}" ]]; then
    echo "MODEL・REVISION・REASONING_EFFORT を start_server.sh に設定してください" >&2
    exit 1
fi

export HF_HOME="${DATA_ROOT}/hf"

cd "$(dirname "$0")"
source "${CONDA_SH}"
conda activate "${CONDA_ENV}"

mkdir -p "${DATA_ROOT}/logs"
python server.py \
    --port "${PORT}" \
    --data-dir "${DATA_ROOT}/jobs" \
    --model "${MODEL}" \
    --revision "${REVISION}" \
    --reasoning-effort "${REASONING_EFFORT}" \
    --max-model-len "${MAX_MODEL_LEN}" \
    --gpu-memory-utilization "${GPU_MEMORY_UTILIZATION}" \
    --max-tokens "${MAX_TOKENS}" \
    2>&1 | tee "${DATA_ROOT}/logs/server_$(date +%Y%m%d_%H%M%S).log"
