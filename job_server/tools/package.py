"""Blackwell 機に送るサーバー一式を作る（Blackwell 機にはリポジトリを置かない）。

commit 済みの内容から、サーバーに必要なファイルだけを job_server_<commit>.tar.gz にまとめる。
作業ツリーではなく git の中身を使うので、Windows で実行しても改行は LF のまま入る。
中に SOURCE_COMMIT（元の commit）と MANIFEST.sha256（各ファイルのハッシュ）を入れ、サーバーは起動時に
これと照らし合わせて、どの commit で動いているか・書き換えられていないかを記録する。

    python job_server/tools/package.py            # HEAD から作る
    python job_server/tools/package.py --commit <hash>
"""
import argparse
import hashlib
import io
import subprocess
import sys
import tarfile
import time
from pathlib import Path

# Blackwell 機で必要なファイル（job_server/ からの相対パス）
FILES = [
    "server.py",
    "app.py",
    "worker.py",
    "job_store.py",
    "engine.py",
    "environment.py",
    "start_server.sh",
    "env.sh",
    "README.md",
    "SETUP_BLACKWELL.md",
]
EXECUTABLE = {"start_server.sh"}


def git(*args: str) -> bytes:
    return subprocess.run(["git", *args], check=True, capture_output=True).stdout


def build(commit: str, out_dir: Path) -> Path:
    full = git("rev-parse", "--verify", f"{commit}^{{commit}}").decode().strip()
    contents = {name: git("show", f"{full}:job_server/{name}") for name in FILES}
    manifest = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in contents.items())
    contents["SOURCE_COMMIT"] = (full + "\n").encode()
    contents["MANIFEST.sha256"] = manifest.encode()

    out = out_dir / f"job_server_{full[:12]}.tar.gz"
    with tarfile.open(out, "w:gz") as tar:
        for name, data in contents.items():
            info = tarfile.TarInfo(f"job_server/{name}")
            info.size = len(data)
            info.mtime = int(time.time())
            info.mode = 0o755 if name in EXECUTABLE else 0o644
            tar.addfile(info, io.BytesIO(data))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--commit", default="HEAD")
    p.add_argument("--out-dir", type=Path, default=Path("."))
    args = p.parse_args()

    dirty = git("status", "--porcelain", "--", "job_server").decode().strip()
    if dirty and args.commit == "HEAD":
        print("warning: job_server/ has uncommitted changes; they are NOT included:\n" + dirty, file=sys.stderr)
    out = build(args.commit, args.out_dir)
    print(f"created {out}")


if __name__ == "__main__":
    main()
