#ifndef LLM_CLIENT_HPP
#define LLM_CLIENT_HPP

#pragma once
#include <string>
#include <string>
#include <vector>
#include "httplib.h"
#include "httplib.h"
#include "nlohmann/json.hpp"

// nlohmann/jsonライブラリの名前空間を省略
using json = nlohmann::json;

// LLMのパラメータを管理する構造体
struct LLMParams {
    std::string model = "openai/gpt-oss-120b";
    std::string system_prompt = "あなたは以下のプロフィールを持つ人物です。この人物になりきって、質問に回答してください。あなたの個人的な意見ではなく、この人物の性格や背景に基づいた考えで回答することが重要です。";
    double temperature = 0.0;
    int seed = 42;
    bool stream = false;
    int max_tokens = 1024;
    double repetition_penalty = 1.1;
    // 必要に応じて他のパラメータを追加
    double top_p = 0.05;
    // std::vector<std::string> stop = {"\n", "。"};
};

//LLMに問い合わせを行う
// std::string queryLLM(const std::string& prompt,const std::string& host, int port);

std::string queryLLM(const std::string& prompt, const std::string& host, int port, const LLMParams& params);

#endif