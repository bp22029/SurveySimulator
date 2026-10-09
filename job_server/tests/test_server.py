import gzip
import sqlite3
import sys
import threading
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from app import create_app  # noqa: E402
from engine import Completion  # noqa: E402
from job_store import JobStore  # noqa: E402
from worker import Worker  # noqa: E402


class RecordingEngine:
    """generate() の呼び出しを記録し、user_prompt に応じた出力を返す。"""

    model = "fake"
    revision = "0" * 40

    def __init__(self):
        self.calls = []
        self.conditions = []
        self.fail_next = False

    def generate(self, pairs, enable_thinking, reasoning_effort, max_tokens):
        self.calls.append([u for _, u in pairs])
        self.conditions.append((enable_thinking, reasoning_effort, max_tokens))
        if self.fail_next:
            self.fail_next = False
            raise RuntimeError("CUDA error (simulated)")
        return [
            Completion(f"<think>t</think><answer>{u}</answer>", "length" if u == "long" else "stop", len(u))
            for _, u in pairs
        ]

    def describe(self):
        return {"engine": "fake"}


@pytest.fixture
def server(tmp_path):
    store = JobStore(tmp_path)
    engine = RecordingEngine()
    worker = Worker(store, engine)
    client = TestClient(create_app(store, lambda: None, {"model": "fake"}, max_model_len=16384))
    yield store, engine, worker, client
    store.close()


def job(client_id="bp22029", sweep=1, users=("1", "2"), enable_thinking=True, reasoning_effort="medium",
        max_tokens=8192):
    return {
        "client_id": client_id,
        "sweep": sweep,
        "enable_thinking": enable_thinking,
        "reasoning_effort": reasoning_effort,
        "max_tokens": max_tokens,
        "requests": [
            {"id": f"{i}_dq2_1", "system_prompt": "s", "user_prompt": u} for i, u in enumerate(users)
        ],
    }


def test_submit_run_and_fetch(server):
    _, engine, worker, client = server
    r = client.post("/jobs", json=job(users=("1", "long")))
    assert r.json() == {"job_id": 1, "created": True}
    assert client.get("/jobs/1").json()["status"] == "queued"
    assert "results" not in client.get("/jobs/1").json()

    assert worker.run_once()
    body = client.get("/jobs/1").json()
    assert body["status"] == "done"
    assert (body["client_id"], body["sweep"], body["n_length"]) == ("bp22029", 1, 1)
    assert (body["enable_thinking"], body["reasoning_effort"], body["max_tokens"]) == (True, "medium", 8192)
    assert body["results"] == [
        {"id": "0_dq2_1", "response": "<think>t</think><answer>1</answer>", "finish_reason": "stop", "n_tokens": 1},
        {"id": "1_dq2_1", "response": "<think>t</think><answer>long</answer>", "finish_reason": "length", "n_tokens": 4},
    ]
    assert engine.calls == [["1", "long"]]
    assert engine.conditions == [(True, "medium", 8192)]


def test_each_job_runs_with_its_own_conditions(server):
    _, engine, worker, client = server
    client.post("/jobs", json=job("bp22029", 1, reasoning_effort="medium", max_tokens=4096))
    client.post("/jobs", json=job("kohai", 1, reasoning_effort="xhigh", max_tokens=12000))
    client.post("/jobs", json=job("kohai", 2, enable_thinking=False, reasoning_effort=None, max_tokens=256))
    while worker.run_once():
        pass
    expected = [(True, "medium", 4096), (True, "xhigh", 12000), (False, None, 256)]
    assert engine.conditions == expected
    jobs = client.get("/jobs").json()["jobs"]
    assert [(j["enable_thinking"], j["reasoning_effort"], j["max_tokens"]) for j in jobs] == expected
    assert client.get("/jobs/3").json()["enable_thinking"] is False


def test_resubmitting_same_sweep_with_different_conditions_is_rejected(server):
    _, _, _, client = server
    client.post("/jobs", json=job(reasoning_effort="medium"))
    assert client.post("/jobs", json=job(reasoning_effort="low")).status_code == 409
    assert client.post("/jobs", json=job(max_tokens=4096)).status_code == 409
    assert client.post("/jobs", json=job(enable_thinking=False, reasoning_effort=None)).status_code == 409
    assert client.post("/jobs", json=job()).json() == {"job_id": 1, "created": False}


def test_result_is_served_gzipped_when_accepted(server):
    store, _, worker, client = server
    client.post("/jobs", json=job())
    worker.run_once()
    r = client.get("/jobs/1", headers={"Accept-Encoding": "gzip"})
    assert r.headers["content-encoding"] == "gzip"
    assert r.json()["status"] == "done"  # httpx が展開する
    assert r.num_bytes_downloaded == store.result_path(1).stat().st_size


def test_jobs_run_one_at_a_time_in_arrival_order(server):
    _, engine, worker, client = server
    client.post("/jobs", json=job("bp22029", 1, users=("a1", "a2")))
    client.post("/jobs", json=job("kohai", 1, users=("b1",)))
    client.post("/jobs", json=job("bp22029", 2, users=("a3",)))
    while worker.run_once():
        pass
    # ジョブを 1 回の generate() に混ぜない
    assert engine.calls == [["a1", "a2"], ["b1"], ["a3"]]


def test_resubmitting_same_sweep_returns_existing_job(server):
    _, engine, worker, client = server
    assert client.post("/jobs", json=job()).json() == {"job_id": 1, "created": True}
    assert client.post("/jobs", json=job()).json() == {"job_id": 1, "created": False}
    worker.run_once()
    assert client.post("/jobs", json=job()).json() == {"job_id": 1, "created": False}
    assert not worker.run_once()
    assert len(engine.calls) == 1


def test_resubmitting_same_sweep_with_different_requests_is_rejected(server):
    _, _, _, client = server
    client.post("/jobs", json=job(users=("1",)))
    r = client.post("/jobs", json=job(users=("2",)))
    assert r.status_code == 409


def test_failed_job_is_requeued_on_resubmit(server):
    _, engine, worker, client = server
    engine.fail_next = True
    client.post("/jobs", json=job())
    worker.run_once()
    body = client.get("/jobs/1").json()
    assert body["status"] == "failed"
    assert "CUDA error" in body["error"]

    assert client.post("/jobs", json=job()).json() == {"job_id": 1, "created": False}
    worker.run_once()
    assert client.get("/jobs/1").json()["status"] == "done"
    [meta] = client.get("/jobs", params={"client_id": "bp22029"}).json()["jobs"]
    assert (meta["status"], meta["attempts"]) == ("done", 2)


def test_restart_requeues_running_job_and_keeps_results(tmp_path):
    store = JobStore(tmp_path)
    store.submit("bp22029", 1, [{"id": "0_q", "system_prompt": "s", "user_prompt": "1"}], True, "medium", 8192)
    store.submit("kohai", 1, [{"id": "0_q", "system_prompt": "s", "user_prompt": "2"}], False, None, 4096)
    Worker(store, RecordingEngine()).run_once()
    store.claim_next()  # job 2 を実行中のままサーバーが止まった
    store.close()

    store = JobStore(tmp_path)
    assert store.recover() == 1
    client = TestClient(create_app(store, lambda: None, {}))
    assert client.get("/jobs/1").json()["status"] == "done"
    assert client.get("/jobs/2").json()["status"] == "queued"
    engine = RecordingEngine()
    Worker(store, engine).run_once()
    assert engine.calls == [["2"]]
    assert engine.conditions == [(False, None, 4096)]
    assert client.get("/jobs/2").json()["results"][0]["response"] == "<think>t</think><answer>2</answer>"
    store.close()


def test_lookup_by_client_and_sweep(server):
    _, _, _, client = server
    client.post("/jobs", json=job("bp22029", 1))
    client.post("/jobs", json=job("kohai", 1))
    client.post("/jobs", json=job("bp22029", 2))
    jobs = client.get("/jobs", params={"client_id": "bp22029", "sweep": 2}).json()["jobs"]
    assert [(j["job_id"], j["status"]) for j in jobs] == [(3, "queued")]
    assert [j["job_id"] for j in client.get("/jobs", params={"client_id": "bp22029"}).json()["jobs"]] == [1, 3]
    assert client.get("/jobs", params={"client_id": "nobody"}).json() == {"jobs": []}


def test_queue_lists_running_and_queued(server):
    store, _, _, client = server
    client.post("/jobs", json=job("bp22029", 1))
    client.post("/jobs", json=job("kohai", 1))
    store.claim_next()
    q = client.get("/queue").json()
    assert [j["job_id"] for j in q["running"]] == [1]
    assert [j["job_id"] for j in q["queued"]] == [2]


@pytest.mark.parametrize(
    "mutate",
    [
        lambda b: b.update(temperature=0.7),                    # サンプリング設定は受け付けない
        lambda b: b["requests"][0].update(max_tokens=10),
        lambda b: b["requests"].append(dict(b["requests"][0])),  # id の重複
        lambda b: b.update(requests=[]),
        lambda b: b.pop("sweep"),
        lambda b: b["requests"][0].update(id=123),               # id は文字列（"personID_questionID"）
        lambda b: b.pop("reasoning_effort"),                     # 推論条件は必ず指定する
        lambda b: b.pop("max_tokens"),
        lambda b: b.update(reasoning_effort="high"),
        lambda b: b.update(reasoning_effort=None),
        lambda b: b.update(max_tokens=0),
        lambda b: b.update(max_tokens=16384),                    # max_model_len 以上
        lambda b: b["requests"][0].update(reasoning_effort="low"),  # リクエストごとには指定できない
        lambda b: b.pop("enable_thinking"),
        lambda b: b.update(enable_thinking="true"),               # 文字列や数値を真偽値に読み替えない
        lambda b: b.update(enable_thinking=1),
        lambda b: b.update(enable_thinking=False),               # 思考なしで reasoning_effort を指定
    ],
)
def test_invalid_submissions_are_rejected(server, mutate):
    _, _, _, client = server
    body = job()
    mutate(body)
    assert client.post("/jobs", json=body).status_code == 422


def test_unknown_job_is_404(server):
    _, _, _, client = server
    assert client.get("/jobs/99").status_code == 404


def test_concurrent_duplicate_submissions_create_one_job(server):
    store, _, _, client = server
    results = []

    def submit():
        results.append(client.post("/jobs", json=job()).json()["job_id"])

    threads = [threading.Thread(target=submit) for _ in range(8)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert set(results) == {1}
    assert len(store.find()) == 1


def test_request_file_is_compressed_json(server):
    store, _, _, client = server
    client.post("/jobs", json=job())
    with gzip.open(store.job_dir(1) / "request.json.gz", "rt", encoding="utf-8") as f:
        assert f.read().startswith('[{"id": "0_dq2_1"')


def test_old_database_is_migrated_and_old_unfinished_jobs_fail(tmp_path):
    # 推論条件をジョブごとに受け取るようになる前の DB（列がない）
    db = sqlite3.connect(tmp_path / "jobs.sqlite3")
    db.execute(
        "CREATE TABLE jobs (job_id INTEGER PRIMARY KEY AUTOINCREMENT, client_id TEXT NOT NULL,"
        " sweep INTEGER NOT NULL, request_hash TEXT NOT NULL, n_requests INTEGER NOT NULL, status TEXT NOT NULL,"
        " attempts INTEGER NOT NULL DEFAULT 0, submitted_at REAL NOT NULL, started_at REAL, finished_at REAL,"
        " n_length INTEGER, error TEXT, UNIQUE (client_id, sweep))"
    )
    db.execute("INSERT INTO jobs (client_id, sweep, request_hash, n_requests, status, submitted_at)"
               " VALUES ('verify', 1, 'old', 1, 'queued', 0)")
    db.commit()
    db.close()
    (tmp_path / "jobs" / "000001").mkdir(parents=True)
    with gzip.open(tmp_path / "jobs" / "000001" / "request.json.gz", "wt", encoding="utf-8") as f:
        f.write('[{"id": "0_q", "system_prompt": "s", "user_prompt": "1"}]')

    store = JobStore(tmp_path)
    client = TestClient(create_app(store, lambda: None, {}))
    old = client.get("/jobs/1").json()
    assert (old["enable_thinking"], old["reasoning_effort"], old["max_tokens"]) == (None, None, None)

    engine = RecordingEngine()
    Worker(store, engine).run_once()
    assert engine.calls == []  # 推論条件の分からないジョブは推論しない
    assert "no enable_thinking/max_tokens" in client.get("/jobs/1").json()["error"]

    # 同じ sweep には投げ直せない（中身のハッシュが変わる）。新しい sweep なら通る
    assert client.post("/jobs", json=job("verify", 1, users=("1",))).status_code == 409
    assert client.post("/jobs", json=job("verify", 2, users=("1",))).json() == {"job_id": 2, "created": True}
    Worker(store, engine).run_once()
    assert engine.conditions == [(True, "medium", 8192)]
    store.close()
