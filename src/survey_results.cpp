//
// Created by bp22029 on 2025/08/15.
//
#include "../include/survey_results.hpp"

#include <iostream>

#include "../include/question.hpp"
#include <vector>
#include <map>

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