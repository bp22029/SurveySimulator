#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include "../include/data_loader.hpp"
#include "../include/prompt_generator.hpp"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "llm_client.hpp"
#include "survey_results.hpp"
#include "simulation_runner.hpp"
#include "individual_response_manager.hpp"
#include "cross_table_generator.hpp"

// nlohmann/jsonを使いやすくするために名前空間を指定
using json = nlohmann::json;

// 応答文字列を解析し、対応するSurveyResultのカウンターを更新する関数



int main() {
    // 合成人口データの読み込み
    std::vector<Person> population = readSyntheticPopulation("../data/2015_001_8+_47356.csv");
    if (population.empty()) {
        return 1;
    }

    // 質問データの読み込み
    std::vector<Question> questions = readQuestions("../data/ssm2015.csv");
    if (questions.empty()) {
        return 1;
    }
    // プロンプトテンプレートの読み込み
    std::string prompt_template = readPromptTemplate();

    //　回答集計の配列を初期化
    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);

    // // 4. シミュレーションの実行
    runSurveySimulation(population, questions, prompt_template, results);
    //runSurveySimulation_Parallel(population, questions, prompt_template, results, 64); // 64スレッドで実行


    // 5．csvからクロス集計を行う
    std::string merged_filename = "../data/merged_population_responses.csv";
    std::ifstream file(merged_filename);
    if (!file.is_open()) {
        std::cerr << "ファイルを開けません: " << merged_filename << std::endl;
        return 1;
    }

    CrossGenderTable cross_gender_table = buildCrossGenderTableFromCsv(merged_filename, questions);
    printCrossGenderTable(cross_gender_table);





    return 0;
}