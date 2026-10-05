"""起動時の環境確認と、実験の記録に残すサーバー情報の収集（設計書 §3.3、§6）。"""
import hashlib
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
    # vLLM 0.30.0 は Qwen3.8 の線形注意（GDN）で batch invariance に対応しておらず、1 では起動しない
    # （"VLLM batch_invariant mode is not supported for GDN_ATTN"、2026-10-05）。0 を明示する
    "VLLM_BATCH_INVARIANT": {"0"},
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
        "git": source_state(Path(__file__).resolve().parent),
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


def source_state(code_dir: Path) -> dict:
    """どの commit のコードで動いているか。

    tools/package.py で作った一式（SOURCE_COMMIT と MANIFEST.sha256 がある）なら、各ファイルのハッシュを
    照らし合わせ、書き換えられたファイルがあれば dirty にする。なければ git から取る。
    """
    commit_file = code_dir / "SOURCE_COMMIT"
    manifest_file = code_dir / "MANIFEST.sha256"
    if not commit_file.exists() or not manifest_file.exists():
        return {"source": "git", **_git_state(code_dir)}

    modified = []
    for line in manifest_file.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        digest, name = line.split(None, 1)
        path = code_dir / name.strip()
        # 改行コードと末尾の改行の違いは無視する（tools/package.py の content_hash と同じ計算）
        normalized = path.read_bytes().replace(b"\r\n", b"\n").rstrip(b"\n") if path.exists() else None
        if normalized is None or hashlib.sha256(normalized).hexdigest() != digest:
            modified.append(name.strip())
    return {
        "source": "package",
        "commit": commit_file.read_text(encoding="utf-8").strip(),
        "dirty": bool(modified),
        "modified": modified,
    }


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
