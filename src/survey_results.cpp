//
// Created by bp22029 on 2025/08/15.
//
#include "../include/survey_results.hpp"
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