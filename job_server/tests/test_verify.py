import gzip
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import verify  # noqa: E402

VERIFY = ROOT / "tools" / "verify.py"


def test_final_answer_takes_last_answer_after_thinking():
    assert verify.final_answer("<think>\n<answer>1</answer>\n</think>\n\n<answer>\n3\n</answer>") == 3
    assert verify.final_answer("<think>\n<answer>1</answer>\n</think>\n no answer") == -1
    assert verify.final_answer("<answer>2</answer> <answer>4</answer>") == 4  # </think> がない


def write(path, responses):
    body = json.dumps({"results": [{"id": i, "response": r} for i, r in responses.items()]})
    if path.suffix == ".gz":
        with gzip.open(path, "wt", encoding="utf-8") as f:
            f.write(body)
    else:
        path.write_text(body, encoding="utf-8")


def compare(a, b):
    return subprocess.run([sys.executable, str(VERIFY), "compare", str(a), str(b)],
                          capture_output=True, text=True, encoding="utf-8")


def test_compare_counts_text_and_final_answer_differences(tmp_path):
    a, b = tmp_path / "a.json.gz", tmp_path / "b.json"
    write(a, {"1_q": "x</think><answer>1</answer>", "2_q": "x</think><answer>2</answer>",
              "3_q": "x</think><answer>3</answer>"})
    write(b, {"1_q": "x</think><answer>1</answer>", "2_q": "y</think><answer>2</answer>",
              "3_q": "y</think><answer>4</answer>"})
    res = compare(a, b)
    assert res.returncode == 1
    assert "common ids: 3  identical: 1  different: 2  different final answer: 1" in res.stdout
    assert "3_q: A=3 B=4" in res.stdout

    assert compare(a, a).returncode == 0
