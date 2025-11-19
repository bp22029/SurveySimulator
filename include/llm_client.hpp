#ifndef LLM_CLIENT_HPP
#define LLM_CLIENT_HPP

#pragma once
#include <string>
#include <string>
#include <vector>
#include "httplib.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <functional> // ★ std::function のために追加

// nlohmann/jsonライブラリの名前空間を省略
using json = nlohmann::json;

// LLMのパラメータを管理する構造体
struct LLMParams {
    std::string model = "openai/gpt-oss-20b";
    std::string system_prompt;
    double temperature = 0.0;
    int seed = 42;
    bool stream = false;
    int max_tokens = 4096;
    double repetition_penalty = 1.1;
    // 必要に応じて他のパラメータを追加
    double top_p = 0.0;
    // std::vector<std::string> stop = {"\n", "。"};
};
struct LLMParamsForPersonality : public LLMParams {
    LLMParamsForPersonality() {
        // 人格推定・BFI2向けの推奨既定値（必要に応じて調整）
        system_prompt =
            "あなたは性格の専門家です。性格の特性を表す数値から性格を推定し、人物像を記述してください。"
            "ビッグファイブの各特性について、1行ずつ説明してください。"
            "総合的な人物像を３行程度で最後に記述してください。";
        temperature = 0.2;
        top_p = 0.05;
        max_tokens = 4096;
        repetition_penalty = 1.1;
        seed = 42;
        stream = false;
        // 必要ならモデルも専用に変更
        // model = "openai/gpt-oss-20b";
    }
};

// 戻り値を格納する構造体を定義
struct LLMResponse {
    std::string content;           // 最終的な回答 (数字など)
    std::string reasoning_content; // 思考プロセス
    bool success = false;          // 通信成功フラグ
};

// ★ queryLLM と同じ「型」を持つ関数ポインタ（std::function）を定義
// using LlmQueryFunc = std::function<std::string(
//     const std::string&,
//     const std::string&,
//     int,
//     const LLMParams&
// )>;

using LlmQueryFunc = std::function<LLMResponse(const std::string&, const std::string&, int, const LLMParams&)>;

//LLMに問い合わせを行う
// std::string queryLLM(const std::string& prompt,const std::string& host, int port);

LLMResponse queryLLM(const std::string& prompt, const std::string& host, int port, const LLMParams& params);

std::string queryLLMForPersonality(const std::string& prompt, const std::string& host, int port, const LLMParams& params);

#endif