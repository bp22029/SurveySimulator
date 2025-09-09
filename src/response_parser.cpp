//
// Created by bp22029 on 2025/09/09.
//
// response_parser.cpp
#include "response_parser.hpp"
#include <iostream>

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
