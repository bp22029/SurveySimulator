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
    std::vector<Person> test_population = readPopulationForTest("../data/verification_population.csv");
    if (test_population.empty()) {
        return 1;
    }

    // 質問データの読み込み
    std::vector<Question> questions = readQuestions("../data/ssm2015.csv");
    if (questions.empty()) {
        return 1;
    }
    // プロンプトテンプレートの読み込み
    //std::string template_path = "../data/prompt_templates/prompt_template.txt";
    std::string template_path = "../data/prompt_templates/prompt_template_BFI2.txt";
    std::string prompt_template = readPromptTemplate(template_path);

    //　回答集計の配列を初期化
    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);

    // 4. シミュレーションの実行
    //runSurveySimulation(population, questions, prompt_template, results);
    //runSurveySimulation_Parallel(population, questions, prompt_template, results, 64); // 64スレッドで実行

    std::string generated_prompt = generatePrompt(prompt_template, test_population[0], questions[0]);
    std::cout << generated_prompt << std::endl;

    // テスト用シミュレーションの実行
    runTestSurveySimulation(test_population, questions, prompt_template);

    // // 5．csvからクロス集計を行う
    // std::string merged_filename = "../data/merged_population_responses.csv";
    // std::ifstream file(merged_filename);
    // if (!file.is_open()) {
    //     std::cerr << "ファイルを開けません: " << merged_filename << std::endl;
    //     return 1;
    // }
    // CrossGenderTable cross_gender_table = buildCrossGenderTableFromCsv(merged_filename, questions);
    // printCrossGenderTable(cross_gender_table);





    return 0;
}