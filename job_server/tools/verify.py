"""job_server の再現性の検証（SETUP_BLACKWELL.md §8、設計書 §5）。標準ライブラリだけで動く。

  # requests.json（DumpPrompts の出力）から一部の人を選んで投げ、結果を保存する
  python verify.py submit --server http://HOST:8000 --requests requests.json \
      --client-id verify --sweep 1 --persons 20 --out r1.json

  # 2つの結果を比べる（共通する id の応答テキストが完全に一致するか）
  python verify.py compare r1.json r2.json

  # 保存した結果の出力トークン数の分布
  python verify.py stats r1.json

client_id と sweep の組はジョブごとに一意なので、同じ入力を何度も推論するときは sweep を変える。
"""
import argparse
import gzip
import json
import sys
import time
import urllib.error
import urllib.request


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
    print(f"submitting {len(chosen_requests)} prompts ({len(chosen)} persons: {chosen[0]} ... {chosen[-1]})")
    res = http("POST", f"{server}/jobs",
               {"client_id": args.client_id, "sweep": args.sweep, "requests": chosen_requests})
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


def cmd_stats(args):
    with open(args.file, encoding="utf-8") as f:
        print_token_stats(json.load(f)["results"])


def cmd_compare(args):
    def load(path):
        with open(path, encoding="utf-8") as f:
            return {r["id"]: r for r in json.load(f)["results"]}

    a, b = load(args.a), load(args.b)
    common = sorted(set(a) & set(b))
    if not common:
        sys.exit("no common ids")
    diff = [i for i in common if a[i]["response"] != b[i]["response"]]
    print(f"common ids: {len(common)}  identical: {len(common) - len(diff)}  different: {len(diff)}")
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
    s.add_argument("--server", required=True)
    s.add_argument("--requests", required=True, help="DumpPrompts の出力")
    s.add_argument("--client-id", required=True)
    s.add_argument("--sweep", type=int, required=True)
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
    if args.command == "submit" and not args.no_wait and not args.out:
        p.error("--out is required unless --no-wait")
    args.func(args)


if __name__ == "__main__":
    main()
