"""推論エンジン。vLLM のオフライン LLM クラスを包む。

temperature などのサンプリング設定はここで固定し、クライアントからは変更できない（設計書 §7）。
思考の有無（enable_thinking）・思考の深さ（reasoning_effort）・出力の上限（max_tokens）は、研究ごとに決めるので
ジョブごとに受け取る。
1ジョブの中では全リクエストで同じ値にする（batch invariance がないので、混ぜると互いの出力に影響しうる）。
"""
from dataclasses import dataclass
from typing import List, Optional, Sequence, Tuple

SEED = 42
REASONING_EFFORTS = ("xhigh", "medium", "low")


@dataclass(frozen=True)
class Completion:
    text: str
    finish_reason: Optional[str]
    n_tokens: int  # 出力のトークン数


class VllmEngine:
    def __init__(
        self,
        model: str,
        revision: str,
        max_model_len: int,
        gpu_memory_utilization: float,
    ):
        # vLLM は GPU 機にしか入っていないので、ここで初めて import する
        from vllm import LLM, SamplingParams

        self._sampling_params_cls = SamplingParams
        self.model = model
        self.revision = revision
        self.max_model_len = max_model_len
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

    def sampling_params(self, max_tokens: int):
        return self._sampling_params_cls(temperature=0.0, top_p=1.0, max_tokens=max_tokens, seed=SEED)

    def format_prompt(
        self, system_prompt: str, user_prompt: str, enable_thinking: bool, reasoning_effort: Optional[str]
    ) -> str:
        messages = [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ]
        # 思考モードと思考の深さはデフォルト値に頼らず明示する（設計書 §3.2）。
        # Qwen3.8 のテンプレートは、思考ありのとき reasoning_effort に応じた指示文をシステムプロンプトの先頭に足す。
        # 思考なしのときは reasoning_effort を見ず、生成プロンプトの末尾を空の <think>\n\n</think> にする
        kwargs = {"reasoning_effort": reasoning_effort} if enable_thinking else {}
        return self.tokenizer.apply_chat_template(
            messages,
            tokenize=False,
            add_generation_prompt=True,
            enable_thinking=enable_thinking,
            **kwargs,
        )

    def generate(
        self,
        pairs: Sequence[Tuple[str, str]],
        enable_thinking: bool,
        reasoning_effort: Optional[str],
        max_tokens: int,
    ) -> List[Completion]:
        prompts = [self.format_prompt(s, u, enable_thinking, reasoning_effort) for s, u in pairs]
        # プロンプトと出力の上限の合計が max_model_len を超えるものがあれば、推論せずに失敗させる
        longest = max(len(self.tokenizer.encode(p, add_special_tokens=False)) for p in prompts)
        if longest + max_tokens > self.max_model_len:
            raise ValueError(
                f"prompt ({longest} tokens) + max_tokens ({max_tokens}) exceeds max_model_len ({self.max_model_len})"
            )
        outputs = self.llm.generate(prompts, self.sampling_params(max_tokens))
        return [
            Completion(o.outputs[0].text, o.outputs[0].finish_reason, len(o.outputs[0].token_ids))
            for o in outputs
        ]

    def describe(self) -> dict:
        sp = self.sampling_params(1)
        return {
            "llm": self.llm_kwargs,
            # enable_thinking・reasoning_effort・max_tokens はジョブごとに指定する（ジョブの記録と結果に残る）
            "sampling": {"temperature": sp.temperature, "top_p": sp.top_p, "seed": sp.seed},
            "kv_cache_tokens": self._kv_cache_tokens(),
            # batch invariance が使えないので、同時に処理する本数を決める設定は出力に影響する。記録して比較する
            "scheduler": self._config_fields(
                "scheduler_config", ["max_num_seqs", "max_num_batched_tokens", "enable_chunked_prefill", "policy"]
            ),
            "cache": self._config_fields("cache_config", ["block_size", "num_gpu_blocks", "mamba_block_size"]),
            # チャットテンプレートの適用結果を残し、思考モードの扱いを起動ログで確認できるようにする
            "chat_template_sample": {
                **{e: self.format_prompt("<SYSTEM>", "<USER>", True, e) for e in REASONING_EFFORTS},
                "no_thinking": self.format_prompt("<SYSTEM>", "<USER>", False, None),
            },
        }

    def _config_fields(self, config_name: str, fields: List[str]) -> dict:
        # 内部属性の場所は vLLM のバージョンで変わるので、取れないものは None
        try:
            config = getattr(self.llm.llm_engine.vllm_config, config_name)
        except Exception:
            return {}
        out = {}
        for name in fields:
            value = getattr(config, name, None)
            out[name] = value if isinstance(value, (int, float, str, bool, type(None))) else str(value)
        return out

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

    def generate(
        self,
        pairs: Sequence[Tuple[str, str]],
        enable_thinking: bool,
        reasoning_effort: Optional[str],
        max_tokens: int,
    ) -> List[Completion]:
        head = f"<think>echo {enable_thinking} {reasoning_effort} {max_tokens}</think>\n<answer>1</answer>\n"
        return [Completion(head + u, "stop", len(u)) for _, u in pairs]

    def describe(self) -> dict:
        return {"engine": "echo"}
