import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))

from environment import source_state  # noqa: E402
import package  # noqa: E402


def test_package_contains_server_files_and_detects_edits(tmp_path, monkeypatch):
    monkeypatch.chdir(ROOT.parent)
    out = package.build("HEAD", tmp_path)
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    assert out.name == f"job_server_{head[:12]}.tar.gz"

    with tarfile.open(out) as tar:
        names = set(tar.getnames())
        assert tar.getmember("job_server/start_server.sh").mode == 0o755
        tar.extractall(tmp_path, filter="data")
    assert {f"job_server/{f}" for f in package.FILES} <= names
    assert "job_server/tests/test_server.py" not in names

    code = tmp_path / "job_server"
    assert b"\r\n" not in (code / "start_server.sh").read_bytes()  # Windows で作っても LF
    state = source_state(code)
    assert state == {"source": "package", "commit": head, "dirty": False, "modified": []}

    (code / "engine.py").write_text("# edited on the server\n", encoding="utf-8")
    state = source_state(code)
    assert state["dirty"] is True
    assert state["modified"] == ["engine.py"]


def test_source_state_falls_back_to_git(tmp_path):
    state = source_state(ROOT)
    assert state["source"] == "git"
    assert len(state["commit"]) == 40
