"""起動時の環境確認と、実験の記録に残すサーバー情報の収集（設計書 §3.3、§6）。"""
import os
import platform
import socket
import subprocess
import sys
import time
from importlib import metadata
from pathlib import Path
from typing import List, Optional

# 起動スクリプトで設定する値。PYTHONHASHSEED は起動時にしか読まれないので、コード内では設定しない
REQUIRED_ENV = {
    "PYTHONHASHSEED": {"42"},
    "VLLM_ENABLE_V1_MULTIPROCESSING": {"0"},
    "CUBLAS_WORKSPACE_CONFIG": {":4096:8"},
    # 採否は速度計測の後に決める（設計書 §8）。どちらでもよいが、明示的に設定されていること
    "VLLM_BATCH_INVARIANT": {"0", "1"},
}

PACKAGES = ("vllm", "torch", "transformers", "fastapi", "uvicorn", "pydantic")


def check_env() -> List[str]:
    problems = []
    for name, allowed in REQUIRED_ENV.items():
        value = os.environ.get(name)
        if value not in allowed:
            problems.append(f"{name}={value!r} (expected one of {sorted(allowed)})")
    return problems


def collect_server_info(engine, args) -> dict:
    return {
        "started_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "hostname": socket.gethostname(),
        "python": sys.version,
        "platform": platform.platform(),
        "packages": {p: _version(p) for p in PACKAGES},
        "torch_cuda": _torch_cuda(),
        "gpu": _nvidia_smi(),
        "git": _git_state(Path(__file__).resolve().parent),
        "env": {name: os.environ.get(name) for name in REQUIRED_ENV},
        "model": engine.model,
        "revision": engine.revision,
        "engine": engine.describe(),
        "args": vars(args),
    }


def _version(package: str) -> Optional[str]:
    try:
        return metadata.version(package)
    except metadata.PackageNotFoundError:
        return None


def _torch_cuda() -> Optional[str]:
    try:
        import torch
        return torch.version.cuda
    except Exception:
        return None


def _nvidia_smi() -> Optional[str]:
    return _run(["nvidia-smi", "--query-gpu=name,driver_version,memory.total", "--format=csv,noheader"])


def _git_state(repo_dir: Path) -> dict:
    status = _run(["git", "-C", str(repo_dir), "status", "--porcelain"])
    return {
        "commit": _run(["git", "-C", str(repo_dir), "rev-parse", "HEAD"]),
        # 未コミットの変更があると、記録した commit とは別の設定で動いていることになる
        "dirty": None if status is None else bool(status),
    }


def _run(cmd: List[str]) -> Optional[str]:
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=30, check=True)
        return out.stdout.strip()
    except Exception:
        return None
