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
#include <iostream>
#include <string>
#include <regex>
#include <optional>

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



// 回答番号を抽出する関数
int extractAnswerFromTags(const std::string& text) {
    // <answer>と</answer>の間にある数字(\d+)を抽出する正規表現
    // \s* は改行やスペースを許容するため
    std::regex re(R"(<answer>\s*(\d+)\s*</answer>)");
    std::smatch match;

    // マッチするか確認
    if (std::regex_search(text, match, re)) {
        try {
            // match[1] が (\d+) の部分（数字）
            return std::stoi(match[1].str());
        } catch (...) {
            return -1; // 数字変換エラー
        }
    }

    return -1; // タグが見つからない場合
}

// 思考ログも抜き出したい場合
std::string extractThinkingLog(const std::string& text) {
    // . は改行を含まないため、改行を含むマッチには工夫が必要ですが、
    // C++のregexでは少し複雑になるため、findを使うのが最速です。
    std::string start_tag = "<thinking>";
    std::string end_tag = "</thinking>";

    size_t start_pos = text.find(start_tag);
    size_t end_pos = text.find(end_tag);

    if (start_pos != std::string::npos && end_pos != std::string::npos) {
        start_pos += start_tag.length();
        return text.substr(start_pos, end_pos - start_pos);
    }
    return "";
}