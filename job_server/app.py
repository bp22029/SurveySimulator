"""ジョブ方式の API（設計書 §3.4）。推論はここでは行わず、Worker に任せる。"""
import gzip
from typing import Callable, List, Literal, Optional

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import FileResponse, JSONResponse, StreamingResponse
from pydantic import BaseModel, ConfigDict, Field, StrictBool, field_validator, model_validator

from job_store import JobStore, RequestMismatch

# 応答に載せるジョブの項目（内部の request_hash などは出さない）
PUBLIC_FIELDS = (
    "job_id", "client_id", "sweep", "enable_thinking", "reasoning_effort", "max_tokens",
    "status", "n_requests", "n_length",
    "attempts", "submitted_at", "started_at", "finished_at",
)


class PromptRequest(BaseModel):
    # 未知の項目（temperature など）は拒否する。temperature などはサーバー側で固定（設計書 §7）
    model_config = ConfigDict(extra="forbid")

    id: str = Field(min_length=1)
    system_prompt: str
    user_prompt: str


class JobSubmission(BaseModel):
    model_config = ConfigDict(extra="forbid")

    client_id: str = Field(min_length=1, max_length=64)
    sweep: int = Field(ge=0)
    # 推論条件は研究ごとに決めるので、ジョブごとに必ず指定してもらう（既定値は置かない）。
    # 1ジョブの中では全リクエストで同じ値になる
    enable_thinking: StrictBool  # "true" や 1 を真偽値に読み替えない
    # 思考ありのときは必須。思考なしのときはテンプレートが使わないので、指定しない（null）
    reasoning_effort: Optional[Literal["xhigh", "medium", "low"]] = None
    max_tokens: int = Field(ge=1)
    requests: List[PromptRequest] = Field(min_length=1)

    @field_validator("requests")
    @classmethod
    def ids_must_be_unique(cls, requests: List[PromptRequest]) -> List[PromptRequest]:
        seen = set()
        for r in requests:
            if r.id in seen:
                raise ValueError(f"duplicate request id: {r.id}")
            seen.add(r.id)
        return requests

    @model_validator(mode="after")
    def effort_only_with_thinking(self) -> "JobSubmission":
        if self.enable_thinking and self.reasoning_effort is None:
            raise ValueError("reasoning_effort is required when enable_thinking is true")
        if not self.enable_thinking and self.reasoning_effort is not None:
            raise ValueError("reasoning_effort must be null when enable_thinking is false (the template ignores it)")
        return self


def public(job: dict) -> dict:
    out = {k: job[k] for k in PUBLIC_FIELDS}
    if job["status"] == "failed":
        out["error"] = job["error"]
    return out


def create_app(
    store: JobStore, notify: Callable[[], None], server_info: dict, max_model_len: Optional[int] = None
) -> FastAPI:
    app = FastAPI(title="SurveySimulator LLM job server")

    @app.post("/jobs")
    def submit_job(submission: JobSubmission):
        # プロンプトの長さを含めた確認は推論の直前に行う（engine.py）。ここでは明らかに収まらないものを断る
        if max_model_len is not None and submission.max_tokens >= max_model_len:
            raise HTTPException(
                status_code=422,
                detail=f"max_tokens ({submission.max_tokens}) must be less than max_model_len ({max_model_len})",
            )
        requests = [r.model_dump() for r in submission.requests]
        try:
            job_id, created = store.submit(
                submission.client_id, submission.sweep, requests,
                submission.enable_thinking, submission.reasoning_effort, submission.max_tokens,
            )
        except RequestMismatch as e:
            raise HTTPException(
                status_code=409,
                detail=f"job {e.job_id} already exists for client_id={submission.client_id}"
                f" sweep={submission.sweep} with different requests or conditions (enable_thinking/reasoning_effort/max_tokens)",
            )
        notify()
        return {"job_id": job_id, "created": created}

    @app.get("/jobs/{job_id}")
    def get_job(job_id: int, request: Request):
        job = store.get(job_id)
        if job is None:
            raise HTTPException(status_code=404, detail=f"job {job_id} not found")
        if job["status"] != "done":
            return public(job)

        # 結果は数百 MB になりうるので、保存済みの gzip をそのまま流す
        path = store.result_path(job_id)
        if "gzip" in request.headers.get("accept-encoding", ""):
            return FileResponse(
                path, media_type="application/json", headers={"Content-Encoding": "gzip"}
            )
        return StreamingResponse(_gunzip_chunks(path), media_type="application/json")

    @app.get("/jobs")
    def list_jobs(client_id: Optional[str] = None, sweep: Optional[int] = None):
        return {"jobs": [public(j) for j in store.find(client_id, sweep)]}

    @app.get("/queue")
    def queue():
        jobs = store.pending()
        return {
            "running": [public(j) for j in jobs if j["status"] == "running"],
            "queued": [public(j) for j in jobs if j["status"] == "queued"],
        }

    @app.get("/info")
    def info():
        return JSONResponse(server_info)

    return app


def _gunzip_chunks(path, chunk_size: int = 1 << 20):
    with gzip.open(path, "rb") as f:
        while chunk := f.read(chunk_size):
            yield chunk
