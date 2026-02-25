from fastapi import FastAPI, Request
from vllm import LLM, SamplingParams
from pydantic import BaseModel 
from typing import List
import uvicorn
import os

import time
import random
import numpy as np
import torch
import argparse

# os.environ["CUDA_VISIBLE_DEVICES"] = "1"
os.environ["VLLM_ENABLE_V1_MULTIPROCESSING"] = "0"

os.environ['CUBLAS_WORKSPACE_CONFIG'] = ':4096:8'
os.environ['PYTHONHASHSEED'] = '42'
os.environ["VLLM_BATCH_INVARIANT"] = "1"

#os.environ["TOKENIZERS_PARALLELISM"] = "false"


app = FastAPI()

class PromptRequest(BaseModel):
    id: int
    system_prompt: str
    user_prompt: str

def set_seed(seed=42):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)

print(">>> Setting seed...")
set_seed(42)

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=8000, help="Port to run the server on")
args = parser.parse_args()

# ★ここで既存のLLMクラス（オフラインエンジン）を初期化
print("Loading model...")
llm = LLM(
    model="Qwen/Qwen3-14B",
    seed=42,
    tensor_parallel_size=1,
    dtype="bfloat16",
    gpu_memory_utilization=0.8,
    # max_model_len=4096,
    max_model_len=8192,
    disable_log_stats=True,
    enforce_eager=True
)
tokenizer = llm.get_tokenizer()
sampling_params = SamplingParams(
    temperature=0.0,
    top_p=1.0,
    # max_tokens=1024,
    max_tokens=4096,
    seed=42
)

print(">>> Server is ready.")
@app.post("/generate")
async def generate_response(data: List[PromptRequest]):
    # data はもう辞書(dict)ではなく、オブジェクトのリストになっています
    prompts = []
    ids = []
    
    for item in data:
        # ★ 辞書アクセス item["key"] ではなく、ドットアクセス item.key に変わります！
        messages = [
            {"role": "system", "content": item.system_prompt},
            {"role": "user", "content": item.user_prompt},
        ]
        
        # テンプレート適用 (tokenizerなどの変数はグローバルにある前提)
        formatted_prompt = tokenizer.apply_chat_template(
            messages, tokenize=False, add_generation_prompt=True
        )
        prompts.append(formatted_prompt)
        ids.append(item.id)

    # 推論実行
    outputs = llm.generate(prompts, sampling_params)

    # 結果の整形
    results = []
    for i, output in enumerate(outputs):
        results.append({
            "id": ids[i],
            "response": output.outputs[0].text
        })

    return results

if __name__ == "__main__":
    # サーバーを起動
    print(f"Starting server on port {args.port}...")
    uvicorn.run(app, host="127.0.0.1", port=args.port)