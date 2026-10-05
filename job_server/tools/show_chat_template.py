"""チャットテンプレートを適用した結果を表示する（GPU 不要。tokenizer だけを読み込む）。

reasoning_effort ごとに、モデルに実際に渡される文字列を確認するために使う。

    python show_chat_template.py --system system.txt --user user.txt
"""
import argparse
from pathlib import Path

from transformers import AutoTokenizer

MODEL = "Qwen/Qwen3.8-27B"
REVISION = "1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--system", type=Path, required=True)
    p.add_argument("--user", type=Path, required=True)
    p.add_argument("--model", default=MODEL)
    p.add_argument("--revision", default=REVISION)
    args = p.parse_args()

    tokenizer = AutoTokenizer.from_pretrained(args.model, revision=args.revision)
    messages = [
        {"role": "system", "content": args.system.read_text(encoding="utf-8")},
        {"role": "user", "content": args.user.read_text(encoding="utf-8")},
    ]
    for effort in ("xhigh", "medium", "low"):
        text = tokenizer.apply_chat_template(
            messages, tokenize=False, add_generation_prompt=True,
            enable_thinking=True, reasoning_effort=effort,
        )
        n_tokens = len(tokenizer(text)["input_ids"])
        print(f"===== reasoning_effort={effort} ({n_tokens} tokens) =====")
        print(text)
        print()


if __name__ == "__main__":
    main()
