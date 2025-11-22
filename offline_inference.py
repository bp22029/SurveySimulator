import argparse
import json
import os
import torch
from vllm import LLM, SamplingParams

def main(args):
    # ------------------------------------------------------------------
    # 1. 再現性のための環境設定 (念のためPyTorchレベルでも固定)
    # ------------------------------------------------------------------
    torch.manual_seed(args.seed)
    if torch.cuda.is_available():
        torch.cuda.manual_seed_all(args.seed)

    # 決定論的アルゴリズムの強制（エラーが出る場合はコメントアウトしてください）
    # os.environ["CUBLAS_WORKSPACE_CONFIG"] = ":4096:8"
    # torch.use_deterministic_algorithms(True)

    print(f"Loading input from: {args.input_file}")

    # ------------------------------------------------------------------
    # 2. 入力データの読み込み
    #    形式: [{"id": 1, "prompt": "...", "system_prompt": "..."}, ...] を想定
    # ------------------------------------------------------------------
    with open(args.input_file, 'r', encoding='utf-8') as f:
        input_data = json.load(f)

    # vLLMに渡すプロンプトのリストを作成
    # (Chat形式で渡す場合と、単純なテキスト結合で渡す場合がありますが、
    #  今回はC++側で整形済みの prompt 文字列が来る想定で実装します)
    prompts = [item["prompt"] for item in input_data]

    # IDの追跡用
    ids = [item.get("id", i) for i, item in enumerate(input_data)]

    # ------------------------------------------------------------------
    # 3. vLLMの初期化 (再現性の要)
    # ------------------------------------------------------------------
    print(f"Initializing vLLM model: {args.model}")

    # SamplingParams: 生成時の確率的挙動を制御
    sampling_params = SamplingParams(
        temperature=0.0,       # 0.0にすることでGreedy Sampling（常に確率最大の単語を選ぶ）にする
        seed=args.seed,        # サンプリングのシード固定
        max_tokens=1024,       # 最大トークン数（必要に応じて変更）
        repetition_penalty=1.1 # 繰り返しペナルティ（必要に応じて）
    )

    # LLMクラスの初期化
    llm = LLM(
        model=args.model,
        seed=args.seed,              # モデル初期化のシード固定
        trust_remote_code=True,      # 必要に応じてTrue
        enforce_eager=True,          # ★重要: CUDA Graphを使わずEagerモードで実行（再現性向上のため）
        tensor_parallel_size=1       # GPU枚数に合わせて変更 (1枚なら1)
        # swap_space=16,             # メモリ不足エラーが出る場合は調整
        # gpu_memory_utilization=0.9 # GPUメモリ使用率の設定
    )

    # ------------------------------------------------------------------
    # 4. 推論実行 (Batch Inference)
    # ------------------------------------------------------------------
    print(f"Generating responses for {len(prompts)} prompts...")
    outputs = llm.generate(prompts, sampling_params)

    # ------------------------------------------------------------------
    # 5. 結果の整形と保存
    # ------------------------------------------------------------------
    results = []
    for i, output in enumerate(outputs):
        generated_text = output.outputs[0].text
        # 入力IDと紐付けて保存
        results.append({
            "id": ids[i],
            "prompt": prompts[i],
            "response": generated_text,
            "finish_reason": output.outputs[0].finish_reason
        })

    print(f"Saving results to: {args.output_file}")
    with open(args.output_file, 'w', encoding='utf-8') as f:
        json.dump(results, f, indent=4, ensure_ascii=False)

    print("Done.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Offline Inference with vLLM for Reproducibility")
    parser.add_argument("--model", type=str, default="openai/gpt-oss-20b", help="Path to the model or HuggingFace model ID")
    parser.add_argument("--input_file", type=str, required=True, help="Path to input JSON file")
    parser.add_argument("--output_file", type=str, required=True, help="Path to output JSON file")
    parser.add_argument("--seed", type=int, default=42, help="Random seed for reproducibility")

    args = parser.parse_args()
    main(args)