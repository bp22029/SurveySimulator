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
    std::vector<Person> population = readSyntheticPopulation("../../data/2015_001_8+_47356.csv");
    if (population.empty()) {
        return 1;
    }

    // テスト用人口データの読み込み
    std::vector<Person> test_population = readPopulationForTest("../../data/verification_population.csv");
    if (test_population.empty()) {
        return 1;
    }

    // 質問データの読み込み
    std::vector<Question> questions = readQuestions("../../data/ssm2015.csv");
    if (questions.empty()) {
        return 1;
    }

    // システムプロンプトのテンプレートの読み込み
    //ビッグファイブ性格特性推定用
    std::string system_template_path_bigfive = "../../data/prompt_templates/prompt_template.txt";
    //BFI2
    std::string system_template_path_bfi2 = "../../data/prompt_templates/prompt_template_BFI2.txt";
    //シュワルツの10価値観
    std::string system_template_path_schwartz = "../../data/prompt_templates/prompt_template_schwartz.txt";
    //シュワルツの価値観(PVQ)
    std::string system_template_path_pvq = "../../data/prompt_templates/prompt_template_PVQ_schwartz.txt";
    //複合型
    std::string system_template_path_complex = "../../data/prompt_templates/prompt_template_complex.txt";


    std::map<std::string, std::string> system_prompt_templates = {
        {"bigfive", readPromptTemplate(system_template_path_bigfive)},
        {"bfi2", readPromptTemplate(system_template_path_bfi2)},
        {"schwartz", readPromptTemplate(system_template_path_schwartz)},
        {"pvq", readPromptTemplate(system_template_path_pvq)},
        {"complex", readPromptTemplate(system_template_path_complex)}
    };

    std::string system_prompt_template = readPromptTemplate(system_template_path_bigfive);

    // ユーザープロンプトのテンプレートの読み込み
    std::string user_template_path = "../../data/prompt_templates/user_prompt_template.txt";
    std::string user_prompt_template = readPromptTemplate(user_template_path);

    //　回答集計の配列を初期化
    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);

    // std::string generated_system_prompt = generatePrompt(system_prompt_template, test_population[0], questions[0]);
    // std::string generated_user_prompt = generatePrompt(user_prompt_template, test_population[0], questions[0]);
    // std::cout << "Generated System Prompt:\n" << generated_system_prompt << std::endl;
    // std::cout << "Generated User Prompt:\n" << generated_user_prompt << std::endl;


    IndividualResponseManager responseManager;

    // 4. シミュレーションの実行
    std::string log_name = "experiment_simulation_log.txt";
    //runSurveySimulation(population, questions, system_prompt_template,user_prompt_template, results, responseManager, &queryLLM, log_name);
    //runSurveySimulation_Parallel(population, questions, prompt_template, results, 64); // 64スレッドで実行
    //runSurveySimulation_Resident(test_population, questions, system_prompt_template,user_prompt_template, responseManager);

    // テスト用シミュレーションの実行
    // runTestSurveySimulation(test_population, questions, system_prompt_templates, user_prompt_template);

    //unsigned int num_threads = 16;

    // runTestSurveySimulation_Parallel(
    //     test_population,
    //     questions,
    //     system_prompt_templates,
    //     user_prompt_template,
    //     num_threads
    // );

    runTestSurveySimulation_Resident(test_population,questions,system_prompt_templates,user_prompt_template);

    // exportResultsToFiles(responseManager,population,questions,
    //                      "../../results/individual_responses.csv",
    //                      "../../data/merged_population_responses.csv");

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