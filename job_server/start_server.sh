#!/bin/bash
# 個性割り当て用LLMサーバーの起動スクリプト（Blackwell 機で実行する）
#
# このファイルの設定値を変えるときは、2人の合意のうえで commit し、一式を作り直して送ること。
# 起動ログとサーバー情報に、一式の元になった commit が記録される（設計書 §6）。
set -euo pipefail

PORT=8000

# --- モデル -------------------------------------------------------------
MODEL="Qwen/Qwen3.8-27B"
REVISION="1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0"   # 2026-08-14 更新の版
# 思考の深さ: xhigh（モデルの既定）/ medium / low。研究として決めて設定する（SETUP_BLACKWELL.md §7）
# 検証のときだけ `REASONING_EFFORT=medium bash start_server.sh` のように起動時に上書きできる
REASONING_EFFORT="${REASONING_EFFORT:-}"

# --- 推論設定 -----------------------------------------------------------
# batch invariance が使えないので、これらを変えると同時に処理する本数が変わり、出力も変わる。
# 検証（SETUP_BLACKWELL.md §10）の前に決め、最適化の途中では変えない
MAX_MODEL_LEN=16384             # プロンプト（約0.7kトークン）＋ MAX_TOKENS が収まること
GPU_MEMORY_UTILIZATION=0.9      # 共有機なので1割は空ける
MAX_TOKENS=8192                 # 仮。回答の長さの分布を見て確定する（SETUP_BLACKWELL.md §8.1）

# --- 再現性のための環境変数（Python コード内では設定しない） -------------
export PYTHONHASHSEED=42
export VLLM_ENABLE_V1_MULTIPROCESSING=0
# vLLM 0.30.0 は Qwen3.8 の線形注意（GDN）で batch invariance に対応しておらず、1 では起動しない
export VLLM_BATCH_INVARIANT=0
export CUBLAS_WORKSPACE_CONFIG=:4096:8

if [[ -z "${MODEL}" || -z "${REVISION}" || -z "${REASONING_EFFORT}" ]]; then
    echo "MODEL・REVISION・REASONING_EFFORT を start_server.sh に設定してください" >&2
    exit 1
fi

cd "$(dirname "$0")"
# 置き場所（~/llmsrv の中の venv・data・cache）を設定し、venv に入る
source ./env.sh
if [[ -z "${VIRTUAL_ENV:-}" ]]; then
    echo "${LLMSRV_ROOT}/venv がありません（SETUP_BLACKWELL.md §3）" >&2
    exit 1
fi

DATA_ROOT="${LLMSRV_ROOT}/data"
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
