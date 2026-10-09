"""job_server の再現性の検証（SETUP_BLACKWELL.md §8、設計書 §5）。標準ライブラリだけで動く。

  # requests.json（DumpPrompts の出力）から一部の人を選んで投げ、結果を保存する
  python verify.py submit --server http://HOST:8000 --requests requests.json \
      --enable-thinking true --reasoning-effort medium --max-tokens 8192 \
      --client-id verify --sweep 1 --persons 20 --out r1.json

  # 2つの結果を比べる（共通する id の応答テキストが完全に一致するか、最終回答の番号が同じか）
  python verify.py compare r1.json r2.json
  python verify.py compare verification/2026-10-08_medium/r1.json.gz r_new.json   # .gz も読める

  # 保存した結果の出力トークン数の分布
  python verify.py stats r1.json

--server を省略すると、環境変数 JOB_SERVER_URL、なければカレントディレクトリの .env の JOB_SERVER_URL を使う。

client_id と sweep の組はジョブごとに一意なので、同じ入力を何度も推論するときは sweep を変える。
enable_thinking・reasoning_effort・max_tokens はジョブの推論条件で、比べる2つの結果では揃えること（compare が表示する）。
思考なし（--enable-thinking false）のときは --reasoning-effort を付けない。
"""
import argparse
import gzip
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request


def default_server(env_file=".env"):
    """C++ の resolveServerUrl と同じ順番：環境変数 JOB_SERVER_URL → .env の JOB_SERVER_URL。"""
    if os.environ.get("JOB_SERVER_URL"):
        return os.environ["JOB_SERVER_URL"]
    try:
        with open(env_file, encoding="utf-8") as f:
            lines = f.read().splitlines()
    except OSError:
        return None
    value = None
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[len("export "):].strip()
        key, sep, val = line.partition("=")
        if sep and key.strip() == "JOB_SERVER_URL":
            val = val.strip()
            if len(val) >= 2 and val[0] == val[-1] and val[0] in "'\"":
                val = val[1:-1]
            value = val or None
    return value


def http(method, url, body=None, timeout=600):
    data = None if body is None else json.dumps(body, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Accept-Encoding", "gzip")
    if data is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as res:
            raw = res.read()
            if res.headers.get("Content-Encoding") == "gzip":
                raw = gzip.decompress(raw)
            return json.loads(raw)
    except urllib.error.HTTPError as e:
        sys.exit(f"{method} {url}: HTTP {e.code}: {e.read()[:500]!r}")


def select(requests, persons=None, person_ids=None, skip=0):
    """id の先頭（person_id）でまとめ、並び順のまま人を選ぶ。"""
    order, by_person = [], {}
    for r in requests:
        pid = r["id"].split("_", 1)[0]
        if pid not in by_person:
            order.append(pid)
            by_person[pid] = []
        by_person[pid].append(r)
    if person_ids:
        chosen = [p for p in order if p in set(person_ids)]
    else:
        chosen = order[skip:skip + persons] if persons else order[skip:]
    return [r for p in chosen for r in by_person[p]], chosen


def cmd_submit(args):
    with open(args.requests, encoding="utf-8") as f:
        requests = json.load(f)["requests"]
    chosen_requests, chosen = select(requests, args.persons, args.person_ids, args.skip)
    server = args.server.rstrip("/")
    print(f"submitting {len(chosen_requests)} prompts ({len(chosen)} persons: {chosen[0]} ... {chosen[-1]}), "
          f"enable_thinking={args.enable_thinking} reasoning_effort={args.reasoning_effort} max_tokens={args.max_tokens}")
    res = http("POST", f"{server}/jobs",
               {"client_id": args.client_id, "sweep": args.sweep, "enable_thinking": args.enable_thinking,
                "reasoning_effort": args.reasoning_effort, "max_tokens": args.max_tokens,
                "requests": chosen_requests})
    job_id = res["job_id"]
    print(f"job_id={job_id} created={res['created']}")
    if args.no_wait:
        return

    while True:
        body = http("GET", f"{server}/jobs/{job_id}")
        if body["status"] in ("done", "failed"):
            break
        time.sleep(args.poll)
    if body["status"] == "failed":
        sys.exit(f"job {job_id} failed:\n{body.get('error')}")

    meta = http("GET", f"{server}/jobs?client_id={args.client_id}&sweep={args.sweep}")["jobs"][0]
    elapsed = meta["finished_at"] - meta["started_at"]
    print(f"done: {len(body['results'])} results, n_length={body['n_length']}, "
          f"elapsed={elapsed:.1f}s ({elapsed / len(chosen):.1f}s/person)")
    print_token_stats(body["results"])
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump({"job": meta, "results": body["results"]}, f, ensure_ascii=False)
    print(f"saved to {args.out}")


def print_token_stats(results):
    """出力トークン数の分布（MAX_TOKENS を決める材料）。"""
    tokens = sorted(r["n_tokens"] for r in results if r.get("n_tokens") is not None)
    if not tokens:
        print("no n_tokens in results")
        return

    def pct(p):
        return tokens[min(len(tokens) - 1, int(p / 100 * len(tokens)))]

    reasons = {}
    for r in results:
        reasons[r.get("finish_reason")] = reasons.get(r.get("finish_reason"), 0) + 1
    print(f"output tokens: n={len(tokens)} mean={sum(tokens) / len(tokens):.0f} "
          f"p50={pct(50)} p90={pct(90)} p99={pct(99)} max={tokens[-1]}")
    for limit in (2048, 4096, 8192, 16384):
        over = sum(1 for t in tokens if t > limit)
        print(f"  > {limit:>5}: {over} ({over / len(tokens):.1%})")
    print(f"finish_reason: {reasons}")


def load_saved(path):
    opener = gzip.open if path.endswith(".gz") else open
    with opener(path, "rt", encoding="utf-8") as f:
        return json.load(f)


def load_results(path):
    return load_saved(path)["results"]


def job_conditions(saved):
    """保存した結果の推論条件。ジョブごとに指定するようになる前の結果では None。"""
    job = saved.get("job", {})
    return job.get("enable_thinking"), job.get("reasoning_effort"), job.get("max_tokens")


def format_conditions(cond):
    return f"enable_thinking={cond[0]} reasoning_effort={cond[1]} max_tokens={cond[2]}"


ANSWER = re.compile(r"<answer>\s*(\d+)\s*</answer>")


def final_answer(response):
    """C++ の extractFinalAnswer と同じ：最後の </think> の後の、最後の <answer>。取れなければ -1。"""
    tail = response.rsplit("</think>", 1)[-1]
    found = ANSWER.findall(tail)
    return int(found[-1]) if found else -1


def cmd_stats(args):
    print_token_stats(load_results(args.file))


def cmd_compare(args):
    saved_a, saved_b = load_saved(args.a), load_saved(args.b)
    cond_a, cond_b = job_conditions(saved_a), job_conditions(saved_b)
    print(f"A: {format_conditions(cond_a)}")
    print(f"B: {format_conditions(cond_b)}")
    # 推論条件が記録されていない（ジョブごとに指定するようになる前の）結果とは比べようがないので知らせない
    if cond_a[0] is not None and cond_b[0] is not None and cond_a != cond_b:
        print("note: the two results were generated with different conditions")
    a = {r["id"]: r for r in saved_a["results"]}
    b = {r["id"]: r for r in saved_b["results"]}
    common = sorted(set(a) & set(b))
    if not common:
        sys.exit("no common ids")
    diff = [i for i in common if a[i]["response"] != b[i]["response"]]
    answer_diff = [i for i in diff if final_answer(a[i]["response"]) != final_answer(b[i]["response"])]
    print(f"common ids: {len(common)}  identical: {len(common) - len(diff)}  different: {len(diff)}  "
          f"different final answer: {len(answer_diff)}")
    for i in answer_diff:
        print(f"  {i}: A={final_answer(a[i]['response'])} B={final_answer(b[i]['response'])}")
    for i in diff[: args.show]:
        x, y = a[i]["response"], b[i]["response"]
        k = next((n for n in range(min(len(x), len(y))) if x[n] != y[n]), min(len(x), len(y)))
        print(f"--- {i}: first difference at char {k}")
        print(f"  A: ...{x[max(0, k - 40):k + 60]!r}")
        print(f"  B: ...{y[max(0, k - 40):k + 60]!r}")
    sys.exit(1 if diff else 0)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="command", required=True)

    s = sub.add_parser("submit")
    s.add_argument("--server", help="省略時は JOB_SERVER_URL（環境変数、なければ .env）")
    s.add_argument("--requests", required=True, help="DumpPrompts の出力")
    s.add_argument("--client-id", required=True)
    s.add_argument("--sweep", type=int, required=True)
    s.add_argument("--enable-thinking", required=True, choices=["true", "false"])
    s.add_argument("--reasoning-effort", choices=["xhigh", "medium", "low"], help="思考ありのときは必須")
    s.add_argument("--max-tokens", type=int, required=True)
    s.add_argument("--persons", type=int, help="先頭から何人分を投げるか（省略時は全員）")
    s.add_argument("--skip", type=int, default=0, help="先頭から何人を飛ばすか")
    s.add_argument("--person-ids", nargs="+", help="投げる人の person_id")
    s.add_argument("--out", help="結果の保存先")
    s.add_argument("--poll", type=int, default=30)
    s.add_argument("--no-wait", action="store_true", help="投入だけして終わる")
    s.set_defaults(func=cmd_submit)

    c = sub.add_parser("compare")
    c.add_argument("a")
    c.add_argument("b")
    c.add_argument("--show", type=int, default=5)
    c.set_defaults(func=cmd_compare)

    st = sub.add_parser("stats", help="保存した結果の出力トークン数の分布")
    st.add_argument("file")
    st.set_defaults(func=cmd_stats)

    args = p.parse_args()
    if args.command == "submit":
        args.server = args.server or default_server()
        if not args.server:
            p.error("--server is required unless JOB_SERVER_URL is set (environment or .env)")
        if not args.no_wait and not args.out:
            p.error("--out is required unless --no-wait")
        args.enable_thinking = args.enable_thinking == "true"
        if args.enable_thinking != (args.reasoning_effort is not None):
            p.error("--reasoning-effort is required with --enable-thinking true and not allowed with false")
    args.func(args)


if __name__ == "__main__":
    main()
