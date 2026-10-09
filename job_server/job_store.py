"""ジョブの保存。メタデータは SQLite、入力と結果は gzip 圧縮した JSON ファイルに置く。

data_dir/
  jobs.sqlite3
  jobs/000001/request.json.gz   投入された requests
  jobs/000001/result.json.gz    GET /jobs/1 の応答本文そのもの（done のときだけ）
"""
import gzip
import hashlib
import json
import os
import sqlite3
import threading
import time
from pathlib import Path
from typing import List, Optional, Tuple

SCHEMA = """
CREATE TABLE IF NOT EXISTS jobs (
    job_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id    TEXT    NOT NULL,
    sweep        INTEGER NOT NULL,
    request_hash TEXT    NOT NULL,
    enable_thinking  INTEGER,
    reasoning_effort TEXT,
    max_tokens   INTEGER,
    n_requests   INTEGER NOT NULL,
    status       TEXT    NOT NULL,
    attempts     INTEGER NOT NULL DEFAULT 0,
    submitted_at REAL    NOT NULL,
    started_at   REAL,
    finished_at  REAL,
    n_length     INTEGER,
    error        TEXT,
    UNIQUE (client_id, sweep)
)
"""

# 推論条件をジョブごとに受け取るようになる前の DB に足す列。それ以前のジョブでは NULL
# （サーバー全体で思考あり・REASONING_EFFORT・MAX_TOKENS を固定していた。値は当時の server_info_*.json にある）
ADDED_COLUMNS = {"enable_thinking": "INTEGER", "reasoning_effort": "TEXT", "max_tokens": "INTEGER"}


def request_hash(
    requests: List[dict], enable_thinking: bool, reasoning_effort: Optional[str], max_tokens: int
) -> str:
    """ジョブの中身のハッシュ。推論条件も含めるので、同じ requests でも条件が違えば別の内容になる。"""
    body = {
        "enable_thinking": enable_thinking,
        "reasoning_effort": reasoning_effort,
        "max_tokens": max_tokens,
        "requests": requests,
    }
    canonical = json.dumps(body, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


class RequestMismatch(Exception):
    """同じ client_id と sweep で、中身の違う requests が投入された。"""

    def __init__(self, job_id: int):
        super().__init__(job_id)
        self.job_id = job_id


class JobStore:
    def __init__(self, data_dir: Path):
        self.data_dir = Path(data_dir)
        (self.data_dir / "jobs").mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._db = sqlite3.connect(
            self.data_dir / "jobs.sqlite3", check_same_thread=False, isolation_level=None
        )
        self._db.row_factory = sqlite3.Row
        self._db.execute("PRAGMA journal_mode=WAL")
        self._db.execute(SCHEMA)
        columns = {r["name"] for r in self._db.execute("PRAGMA table_info(jobs)")}
        for name, sql_type in ADDED_COLUMNS.items():
            if name not in columns:
                self._db.execute(f"ALTER TABLE jobs ADD COLUMN {name} {sql_type}")

    def close(self) -> None:
        self._db.close()

    def job_dir(self, job_id: int) -> Path:
        return self.data_dir / "jobs" / f"{job_id:06d}"

    def result_path(self, job_id: int) -> Path:
        return self.job_dir(job_id) / "result.json.gz"

    # --- 投入 -------------------------------------------------------------

    def submit(
        self,
        client_id: str,
        sweep: int,
        requests: List[dict],
        enable_thinking: bool,
        reasoning_effort: Optional[str],
        max_tokens: int,
    ) -> Tuple[int, bool]:
        """ジョブを登録し (job_id, 新規に作ったか) を返す。

        同じ (client_id, sweep) が既にあれば、中身（requests と推論条件）が同じなら既存の job_id を返す
        （POST のリトライで二重登録しない）。failed のジョブなら queued に戻して再実行する。
        中身が違えば RequestMismatch。
        """
        digest = request_hash(requests, enable_thinking, reasoning_effort, max_tokens)
        payload = gzip.compress(json.dumps(requests, ensure_ascii=False).encode("utf-8"))
        with self._lock:
            row = self._db.execute(
                "SELECT * FROM jobs WHERE client_id = ? AND sweep = ?", (client_id, sweep)
            ).fetchone()
            if row is not None:
                if row["request_hash"] != digest:
                    raise RequestMismatch(row["job_id"])
                if row["status"] == "failed":
                    self._db.execute(
                        "UPDATE jobs SET status = 'queued', started_at = NULL, finished_at = NULL,"
                        " error = NULL WHERE job_id = ?",
                        (row["job_id"],),
                    )
                return row["job_id"], False

            # 入力ファイルを書き終えてから COMMIT する。途中で落ちても、ファイルのない行は残らない
            self._db.execute("BEGIN IMMEDIATE")
            try:
                cur = self._db.execute(
                    "INSERT INTO jobs (client_id, sweep, request_hash, enable_thinking, reasoning_effort, max_tokens,"
                    " n_requests, status, submitted_at) VALUES (?, ?, ?, ?, ?, ?, ?, 'queued', ?)",
                    (client_id, sweep, digest, int(enable_thinking), reasoning_effort, max_tokens, len(requests),
                     time.time()),
                )
                job_id = cur.lastrowid
                _write_atomic(self.job_dir(job_id) / "request.json.gz", payload)
                self._db.execute("COMMIT")
            except BaseException:
                self._db.execute("ROLLBACK")
                raise
            return job_id, True

    # --- ワーカー側 -------------------------------------------------------

    def recover(self) -> int:
        """再起動時に、実行中のまま止まったジョブを queued に戻す。戻した件数を返す。"""
        with self._lock:
            cur = self._db.execute(
                "UPDATE jobs SET status = 'queued', started_at = NULL WHERE status = 'running'"
            )
            return cur.rowcount

    def claim_next(self) -> Optional[dict]:
        """job_id が最も小さい queued のジョブを running にして返す。"""
        with self._lock:
            row = self._db.execute(
                "SELECT * FROM jobs WHERE status = 'queued' ORDER BY job_id LIMIT 1"
            ).fetchone()
            if row is None:
                return None
            self._db.execute(
                "UPDATE jobs SET status = 'running', started_at = ?, attempts = attempts + 1"
                " WHERE job_id = ?",
                (time.time(), row["job_id"]),
            )
            return self._get(row["job_id"])

    def load_requests(self, job_id: int) -> List[dict]:
        with gzip.open(self.job_dir(job_id) / "request.json.gz", "rt", encoding="utf-8") as f:
            return json.load(f)

    def complete(self, job_id: int, results: List[dict]) -> dict:
        job = self.get(job_id)
        n_length = sum(1 for r in results if r["finish_reason"] == "length")
        body = {
            "job_id": job_id,
            "client_id": job["client_id"],
            "sweep": job["sweep"],
            "enable_thinking": job["enable_thinking"],
            "reasoning_effort": job["reasoning_effort"],
            "max_tokens": job["max_tokens"],
            "status": "done",
            "n_requests": job["n_requests"],
            "n_length": n_length,
            "results": results,
        }
        data = gzip.compress(json.dumps(body, ensure_ascii=False).encode("utf-8"))
        _write_atomic(self.result_path(job_id), data)
        with self._lock:
            self._db.execute(
                "UPDATE jobs SET status = 'done', finished_at = ?, n_length = ? WHERE job_id = ?",
                (time.time(), n_length, job_id),
            )
            return self._get(job_id)

    def fail(self, job_id: int, error: str) -> dict:
        with self._lock:
            self._db.execute(
                "UPDATE jobs SET status = 'failed', finished_at = ?, error = ? WHERE job_id = ?",
                (time.time(), error, job_id),
            )
            return self._get(job_id)

    # --- 参照 -------------------------------------------------------------

    def get(self, job_id: int) -> Optional[dict]:
        with self._lock:
            return self._get(job_id)

    def find(self, client_id: Optional[str] = None, sweep: Optional[int] = None) -> List[dict]:
        sql, params = "SELECT * FROM jobs WHERE 1 = 1", []
        if client_id is not None:
            sql += " AND client_id = ?"
            params.append(client_id)
        if sweep is not None:
            sql += " AND sweep = ?"
            params.append(sweep)
        with self._lock:
            return [_row(r) for r in self._db.execute(sql + " ORDER BY job_id", params)]

    def pending(self) -> List[dict]:
        with self._lock:
            rows = self._db.execute(
                "SELECT * FROM jobs WHERE status IN ('running', 'queued') ORDER BY job_id"
            )
            return [_row(r) for r in rows]

    def _get(self, job_id: int) -> Optional[dict]:
        row = self._db.execute("SELECT * FROM jobs WHERE job_id = ?", (job_id,)).fetchone()
        return _row(row) if row else None


def _row(row: sqlite3.Row) -> dict:
    job = dict(row)
    # SQLite には真偽値がないので 0/1 で保存している
    if job.get("enable_thinking") is not None:
        job["enable_thinking"] = bool(job["enable_thinking"])
    return job


def _write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    with open(tmp, "wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)
