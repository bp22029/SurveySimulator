"""推論エンジン。vLLM のオフライン LLM クラスを包む。

サンプリング設定はここで固定し、クライアントからは変更できない（設計書 §7）。
"""
from dataclasses import dataclass
from typing import List, Optional, Sequence, Tuple

SEED = 42


@dataclass(frozen=True)
class Completion:
    text: str
    finish_reason: Optional[str]


class VllmEngine:
    def __init__(
        self,
        model: str,
        revision: str,
        max_model_len: int,
        gpu_memory_utilization: float,
        max_tokens: int,
        reasoning_effort: str,
    ):
        # vLLM は GPU 機にしか入っていないので、ここで初めて import する
        from vllm import LLM, SamplingParams

        self.model = model
        self.revision = revision
        self.reasoning_effort = reasoning_effort
        self.llm_kwargs = dict(
            model=model,
            revision=revision,
            tokenizer_revision=revision,
            seed=SEED,
            tensor_parallel_size=1,
            dtype="bfloat16",
            gpu_memory_utilization=gpu_memory_utilization,
            max_model_len=max_model_len,
            disable_log_stats=True,
            enforce_eager=True,
            enable_prefix_caching=False,
            # Qwen3.8 は画像・動画も扱えるモデル。テキストだけ使うので画像エンコーダを読み込まない
            language_model_only=True,
            # MTP（投機的デコーディング）は使わない。speculative_config は指定しないこと
        )
        self.llm = LLM(**self.llm_kwargs)
        self.tokenizer = self.llm.get_tokenizer()
        self.sampling_params = SamplingParams(
            temperature=0.0,
            top_p=1.0,
            max_tokens=max_tokens,
            seed=SEED,
        )

    def format_prompt(self, system_prompt: str, user_prompt: str) -> str:
        messages = [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ]
        # 思考モードと思考の深さはデフォルト値に頼らず明示する（設計書 §3.2）。
        # Qwen3.8 のテンプレートは reasoning_effort に応じた指示文をシステムプロンプトの先頭に足す
        return self.tokenizer.apply_chat_template(
            messages,
            tokenize=False,
            add_generation_prompt=True,
            enable_thinking=True,
            reasoning_effort=self.reasoning_effort,
        )

    def generate(self, pairs: Sequence[Tuple[str, str]]) -> List[Completion]:
        prompts = [self.format_prompt(s, u) for s, u in pairs]
        outputs = self.llm.generate(prompts, self.sampling_params)
        return [Completion(o.outputs[0].text, o.outputs[0].finish_reason) for o in outputs]

    def describe(self) -> dict:
        sp = self.sampling_params
        return {
            "llm": self.llm_kwargs,
            "sampling": {
                "temperature": sp.temperature,
                "top_p": sp.top_p,
                "max_tokens": sp.max_tokens,
                "seed": sp.seed,
            },
            "enable_thinking": True,
            "reasoning_effort": self.reasoning_effort,
            "kv_cache_tokens": self._kv_cache_tokens(),
            # チャットテンプレートの適用結果を残し、思考モードの扱いを起動ログで確認できるようにする
            "chat_template_sample": self.format_prompt("<SYSTEM>", "<USER>"),
        }

    def _kv_cache_tokens(self) -> Optional[int]:
        # 内部属性の場所は vLLM のバージョンで変わるので、取れなければ None
        try:
            cache_config = self.llm.llm_engine.vllm_config.cache_config
            return int(cache_config.num_gpu_blocks * cache_config.block_size)
        except Exception:
            return None


class EchoEngine:
    """GPU のない環境で API を動かすための代用エンジン。"""

    model = "echo"
    revision = "none"

    def generate(self, pairs: Sequence[Tuple[str, str]]) -> List[Completion]:
        return [Completion(f"<think>echo</think>\n<answer>1</answer>\n{u}", "stop") for _, u in pairs]

    def describe(self) -> dict:
        return {"engine": "echo"}
