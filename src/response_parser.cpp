//
// Created by bp22029 on 2025/09/09.
//
// response_parser.cpp
#include "response_parser.hpp"
#include <iostream>
#include <regex>    // 正規表現を扱うために必要
#include <stdexcept> // std::stoi の例外処理のために必要
#include <cctype>
#include <string>

using json = nlohmann::json;

int extractChoiceNumber(const std::string& json_response) {
    try {
        json parsed = json::parse(json_response);

        // 配列の最初の要素から choice_number を取得
        if (parsed.is_array() && !parsed.empty()) {
            auto first_item = parsed[0];
            if (first_item.contains("final_answer") &&
                first_item["final_answer"].contains("choice_number")) {
                return first_item["final_answer"]["choice_number"].get<int>();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON解析エラー: " << e.what() << std::endl;
    }

    return -1; // エラーの場合
}

int parseLlmAnswer(const std::string& llmResponse) {
    // 最小限の安全なエラー処理: 空でないことと、最初の文字が数字であることを確認
    if (llmResponse.empty() || !std::isdigit(static_cast<unsigned char>(llmResponse[0]))) {
        std::cerr << "parseLlmAnswerエラー: 入力が1文字の数字ではありません。入力: " << llmResponse << std::endl;
        return -1;
    }

    // 文字 '0' から '9' はASCIIコードで連続していることを利用して、高速に整数へ変換
    return llmResponse[0] - '0';
}
