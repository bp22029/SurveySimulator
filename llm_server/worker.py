"""GPU を使う唯一のスレッド。キューから 1 ジョブずつ取り出し、1 ジョブにつき 1 回 generate() を呼ぶ。"""
import logging
import threading
import time
import traceback

from job_store import JobStore

log = logging.getLogger("llm_server.worker")


class Worker(threading.Thread):
    def __init__(self, store: JobStore, engine, poll_interval: float = 5.0):
        super().__init__(name="inference-worker", daemon=True)
        self.store = store
        self.engine = engine
        self.poll_interval = poll_interval
        self._wake = threading.Event()
        self._stop_event = threading.Event()

    def notify(self) -> None:
        self._wake.set()

    def stop(self) -> None:
        self._stop_event.set()
        self._wake.set()

    def run(self) -> None:
        while not self._stop_event.is_set():
            self._wake.clear()
            if not self.run_once():
                self._wake.wait(self.poll_interval)

    def run_once(self) -> bool:
        """queued のジョブを 1 件処理する。処理するジョブがなければ False。"""
        job = self.store.claim_next()
        if job is None:
            return False

        job_id = job["job_id"]
        log.info(
            "job start: job_id=%d client_id=%s sweep=%d n_requests=%d attempt=%d",
            job_id, job["client_id"], job["sweep"], job["n_requests"], job["attempts"],
        )
        t0 = time.time()
        try:
            requests = self.store.load_requests(job_id)
            completions = self.engine.generate(
                [(r["system_prompt"], r["user_prompt"]) for r in requests]
            )
            if len(completions) != len(requests):
                raise RuntimeError(
                    f"output count mismatch: {len(completions)} outputs for {len(requests)} requests"
                )
            results = [
                {"id": r["id"], "response": c.text, "finish_reason": c.finish_reason}
                for r, c in zip(requests, completions)
            ]
            done = self.store.complete(job_id, results)
            log.info(
                "job done: job_id=%d client_id=%s sweep=%d n_requests=%d n_length=%d"
                " submitted_at=%s started_at=%s finished_at=%s elapsed=%.1fs",
                job_id, done["client_id"], done["sweep"], done["n_requests"], done["n_length"],
                _fmt(done["submitted_at"]), _fmt(done["started_at"]), _fmt(done["finished_at"]),
                time.time() - t0,
            )
        except Exception:
            error = traceback.format_exc()
            self.store.fail(job_id, error)
            log.error("job failed: job_id=%d\n%s", job_id, error)
        return True


def _fmt(ts) -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(ts)) if ts else "-"
