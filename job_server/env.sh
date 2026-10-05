# Blackwell 機での置き場所をまとめて設定する。ターミナルでも起動スクリプトでも、最初に読み込む：
#
#   source ~/llmsrv/job_server/env.sh
#
# すべてを job_server/ の1つ上（~/llmsrv）の中に置き、ホームの .bashrc や ~/.cache は使わない。
# 撤去するときは ~/llmsrv を消すだけでよい。

LLMSRV_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export LLMSRV_ROOT

export PATH="${LLMSRV_ROOT}/bin:${PATH}"                 # uv 本体
export UV_PYTHON_INSTALL_DIR="${LLMSRV_ROOT}/python"     # uv が入れる Python
export XDG_CACHE_HOME="${LLMSRV_ROOT}/cache"             # uv・vLLM・FlashInfer などのキャッシュ
export TRITON_CACHE_DIR="${LLMSRV_ROOT}/cache/triton"
export CUDA_CACHE_PATH="${LLMSRV_ROOT}/cache/nv"
export HF_HOME="${LLMSRV_ROOT}/data/hf"                  # モデル

if [[ -f "${LLMSRV_ROOT}/venv/bin/activate" ]]; then
    source "${LLMSRV_ROOT}/venv/bin/activate"
fi
