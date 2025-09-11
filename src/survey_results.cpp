//
// Created by bp22029 on 2025/08/15.
//
#include "../include/survey_results.hpp"

#include <iostream>

#include "../include/question.hpp"
#include <vector>
#include <map>
#include <fstream>
#include <string>
#include "httplib.h"
#include "nlohmann/json.hpp"


void initializeSurveyResults(std::vector<SurveyResult>& results,const std::vector<Question>& questions) {
    // 1. 集計用ベクターの初期化
    for (const auto& q : questions) {
        SurveyResult res;
        res.question_id = q.id;
        res.question_text = q.text;

        // 各選択肢のカウンターを0で初期化
        for (int i = 0; i < q.choices.size(); ++i) {
            int choice_number = i + 1;
            res.answer_counts[choice_number] = 0;
        }
        results.push_back(res);
    }
}

void parseAndRecordAnswer(const std::string& response_str, const Question& question, SurveyResult& result) {
    int answer_choice = -1;

    try {
        answer_choice = std::stoi(response_str);
    } catch (const std::exception& e) {
        std::cerr << "エラー: 応答 '" << response_str << "' の解析に失敗しました。詳細: " << e.what() << std::endl;
        // 必要なら、無効回答用のカウンターを増やすなどの処理を追加
        // result.invalid_count++;
        return; // エラーなので、ここで処理を終了
    }

    // 変換された数値が、有効な選択肢の範囲内かチェック
    if (answer_choice >= 1 && answer_choice <= question.choices.size()) {
        result.answer_counts[answer_choice]++;
        std::cout << "  > 集計成功: 質問[" << result.question_id << "] - 選択肢" << answer_choice << "に1票追加" << std::endl;
    } else {
        std::cerr << "  > 集計失敗: 回答 " << answer_choice << " は有効な選択肢の範囲外です。" << std::endl;
        // こちらも無効回答としてカウント
        // result.invalid_count++;
    }
}

void exportSurveyResultsToCSV(const std::vector<SurveyResult>& results, const std::string& filename) {
    std::ofstream csv_file(filename);
    if (!csv_file.is_open()) {
        std::cerr << "CSVファイルのオープンに失敗しました: " << filename << std::endl;
        return;
    }

    // ヘッダー行の書き込み
    csv_file << "question_id,full_question_text,choice_1_count,choice_2_count,choice_3_count,choice_4_count,choice_5_count\n";

    for (const auto& result : results) {
        // 質問IDと質問文を書き込む
        csv_file << result.question_id << ",\"" << result.question_text << "\"";

        // 選択肢のカウントを書き込む（最大5列）
        for (int i = 1; i <= 5; ++i) {
            csv_file << ",";
            auto it = result.answer_counts.find(i);
            if (it != result.answer_counts.end()) {
                csv_file << it->second;
            }
        }
        csv_file << "\n";
    }

    csv_file.close();
    std::cout << "集計結果をCSVファイルに書き込みました: " << filename << std::endl;
}