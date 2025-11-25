import os
os.environ["CUDA_VISIBLE_DEVICES"] = "1"

# --- 【対策1】 再現性のための環境変数（importより前に書くこと！） ---
os.environ['VLLM_DETERMINISTIC_OPS'] = '1'
os.environ['CUBLAS_WORKSPACE_CONFIG'] = ':4096:8'
os.environ['PYTHONHASHSEED'] = '42'

import json
import time
import random
import numpy as np
import torch
from vllm import LLM, SamplingParams

# パス設定（あなたの環境に合わせてください）
BRIDGE_DIR = "/home/bp22029/vllm_bridge"
REQUEST_FILE = os.path.join(BRIDGE_DIR, "bridge_request.json")
RESPONSE_FILE = os.path.join(BRIDGE_DIR, "bridge_response.json")

def set_seed(seed=42):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)

def main():
    set_seed(42)
    print(">>> [Python] 環境変数を設定しました(VLLM_DETERMINISTIC_OPS=1)。モデルをロード中...")

    # モデル初期化
    llm = LLM(
        model="google/gemma-3-12b-it",
        tensor_parallel_size=1,
        dtype="bfloat16", # まずはこれで。ダメなら "float16"
        seed=42,
        gpu_memory_utilization=0.9,
        max_model_len=4096,
        disable_log_stats=True
    )

    # トークナイザー取得
    tokenizer = llm.get_tokenizer()

    # --- 【対策2】 SamplingParamsにもシードを設定 ---
    sampling_params = SamplingParams(
        temperature=0.0,
        top_p=1.0,
        max_tokens=512,
        seed=42
    )


    print(f">>> [Python] 準備完了！ '{REQUEST_FILE}' を監視しています...")

    while True:
        if os.path.exists(REQUEST_FILE):
            try:
                time.sleep(0.05)

                with open(REQUEST_FILE, "r", encoding="utf-8") as f:
                    data = json.load(f)

                print(f">>> [Python] {len(data)} 件受信。")
                start_time = time.time()

                prompts = []
                for item in data:
                    messages = [
                        {"role": "system", "content": item["system_prompt"]},
                        {"role": "user", "content": item["user_prompt"]},
                    ]
                    formatted_prompt = tokenizer.apply_chat_template(
                        messages,
                        tokenize=False,
                        add_generation_prompt=True
                    )
                    prompts.append(formatted_prompt)

                outputs = llm.generate(prompts, sampling_params)

                results = []
                for i, output in enumerate(outputs):
                    results.append({
                        "id": data[i]["id"],
                        "response": output.outputs[0].text
                    })

                with open(RESPONSE_FILE, "w", encoding="utf-8") as f:
                    json.dump(results, f, ensure_ascii=False, indent=2)

                print(f">>> [Python] 完了 ({time.time() - start_time:.2f}s)")
                os.remove(REQUEST_FILE)

            except Exception as e:
                print(f"Error: {e}")
                if os.path.exists(REQUEST_FILE):
                    os.rename(REQUEST_FILE, REQUEST_FILE + ".error")
        else:
            time.sleep(0.1)

if __name__ == "__main__":
    main()