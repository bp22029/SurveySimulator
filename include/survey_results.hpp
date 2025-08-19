#ifndef SURVEY_RESULT_HPP
#define SURVEY_RESULT_HPP

#include <string>
#include <map>
#include <vector>
#include "question.hpp"

struct SurveyResult {
    std::string question_id;
    std::string question_text; // CSV出力時に質問文も併記すると便利
    std::map<int, int> answer_counts; // 各選択肢のカウンター（内側はmapが便利）
};

void initializeSurveyResults(std::vector<SurveyResult>& results,const std::vector<Question>& questions);

void parseAndRecordAnswer(const std::string& response_str, const Question& question, SurveyResult& result);

#endif