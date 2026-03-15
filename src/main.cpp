#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
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
#include "csv_comparer.hpp"
#include "globals.hpp"
#include <time.h>
#include "experiment_runner.hpp"
#include "experiment_runner_parallel.hpp"
#include "optimization_manager.hpp"



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
    //std::string system_template_path_complex = "../../data/prompt_templates/prompt_template_complex.txt";

    //システムプロンプトのテンプレート群
    std::map<std::string, std::string> system_prompt_templates = {
        {"bigfive", readPromptTemplate(system_template_path_bigfive)},
        // {"bfi2", readPromptTemplate(system_template_path_bfi2)},
        // {"schwartz", readPromptTemplate(system_template_path_schwartz)},
        // {"pvq", readPromptTemplate(system_template_path_pvq)},
    };

    std::string system_template_path_qwen_bigfive = "../../data/prompt_templates/forQwen/qwen_bigfive.txt";
    std::string system_template_path_qwen_bfi2 = "../../data/prompt_templates/forQwen/qwen_bfi2.txt";
    std::string system_template_path_qwen_schwartz = "../../data/prompt_templates/forQwen/qwen_schwartz.txt";
    std::string system_template_path_qwen_pvq = "../../data/prompt_templates/forQwen/qwen_pvq.txt";


    std::map<std::string, std::string> system_prompt_templates_forQwen = {
        {"bigfive", readPromptTemplate(system_template_path_qwen_bigfive)},
        {"bfi2", readPromptTemplate(system_template_path_qwen_bfi2)},
        {"schwartz", readPromptTemplate(system_template_path_qwen_schwartz)},
        {"pvq", readPromptTemplate(system_template_path_qwen_pvq)},
    };

    //仮の一つのテンプレートを読み込む　本番実験用(BigFiveを採用)
    //std::string system_prompt_template = readPromptTemplate(system_template_path_bigfive);
    std::string system_prompt_template_for_Qwen = readPromptTemplate(system_template_path_qwen_bfi2);

    // ユーザープロンプトのテンプレートの読み込み
    std::string user_template_path = "../../data/prompt_templates/user_prompt_template.txt";
    std::string user_prompt_template = readPromptTemplate(user_template_path);

    //　回答集計の配列を初期化
    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);

    std::string test_prompt = generatePrompt(system_prompt_template_for_Qwen, population[0], questions[0]);
    std::cout << "Generated Prompt for Test Person 0 and Question 0:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << test_prompt << std::endl;
    std::cout << "----------------------------------------" << std::endl;


    //検証用のシミュレーション　オフライン
    //runTestSurveySimulation_Resident(test_population,questions,system_prompt_templates,user_prompt_template);

    //シミュレーションの実行
    //std::string log_name = "experiment_simulation_log.txt";
    //runSurveySimulation(population, questions, system_prompt_template,user_prompt_template, results, responseManager, &queryLLM);
    //runSurveySimulation_Parallel(population, questions, prompt_template, results, 64); // 64スレッドで実行
    //
    // // 本番用実験　オフライン推論
    //   IndividualResponseManager responseManager;
    //   runSurveySimulation_Resident(population, questions, system_prompt_template,user_prompt_template, responseManager);
    //
    //   responseManager.printSummary();
    //   exportResultsToFiles(responseManager,population,questions,
    //                          "../../results/individual_responses_BigFive_03.csv",
    //                          "../../data/merged_population_responses_BigFive_03.csv");
    //
    //   IndividualResponseManager responseManager2;
    //   runSurveySimulation_Resident(population, questions, system_prompt_template,user_prompt_template, responseManager2);
    //
    //   responseManager2.printSummary();
    //   exportResultsToFiles(responseManager2,population,questions,
    //                          "../../results/individual_responses_BigFive_04.csv",
    //                          "../../data/merged_population_responses_BigFive_04.csv");


    // テスト用シミュレーションの実行
    //runTestSurveySimulation(test_population, questions, system_prompt_templates, user_prompt_template);

    //unsigned int num_threads = 16;

    // runTestSurveySimulation_Parallel(
    //     test_population,
    //     questions,
    //     system_prompt_templates,
    //     user_prompt_template,
    //     num_threads
    // );

    //
    // // 5．csvからクロス集計を行う
    // std::string merged_filename = "../../data/merged_population_responses_BigFive_01.csv";
    // std::ifstream file(merged_filename);
    // if (!file.is_open()) {
    //     std::cerr << "ファイルを開けません: " << merged_filename << std::endl;
    //     return 1;
    // }
    // CrossGenderTable cross_gender_table = buildCrossGenderTableFromCsv(merged_filename, questions);
    // printCrossGenderTable(cross_gender_table);




    //統合したcsvから人口データを読み込む
     std::string merged_filename = "../../data/parallel_merged_20260117_014011.csv";
     std::vector<Person> merged_population = readPopulationFromMergedCSV(merged_filename);
     if (merged_population.empty()) {
         return 1;
     }

    //verificationReproducibility(test_population,questions,system_prompt_templates_forQwen,user_prompt_template);

    // std::string main_file_01 = "../../data/merged_population_responses_BigFive_01.csv";
    // std::string main_file_02 = "../../data/merged_population_responses_BigFive_02.csv";
    // std::string main_file_03 = "../../data/merged_population_responses_BigFive_03.csv";
    // std::string main_file_04 = "../../data/merged_population_responses_BigFive_04.csv";
    //
    // std::vector<std::string> fileList = {
    //     main_file_01, main_file_02, main_file_03, main_file_04
    // };
    // std::cout << "Starting comparison of " << fileList.size() << " main files..." << std::endl;
    // std::cout << "--------------------------------------------------" << std::endl;
    //
    // //総当たりで比較する二重ループ
    // // i: 比較元のインデックス
    // for (size_t i = 0; i < fileList.size(); ++i) {
    //
    //     // j: 比較先のインデックス
    //     // j = i + 1 から始めることで、以下の無駄を省いています：
    //     // 1. 自分自身との比較 (test_file0 vs test_file0)
    //     // 2. 重複した逆パターンの比較 (0 vs 1 をやった後に 1 vs 0 をやるなど)
    //     for (size_t j = i + 1; j < fileList.size(); ++j) {
    //
    //         // 関数呼び出し
    //         printCsvComparisonResult(fileList[i], fileList[j]);
    //     }
    // }
    // std::cout << "--------------------------------------------------" << std::endl;
    // std::cout << "All comparisons finished of main files." << std::endl;

    std::string model_name = "qwen3-14b";//適宜変更

    IndividualResponseManager responseManager;
    OptimizationManager optManager;
    //runSurveySimulation_Resident(merged_population, questions, system_prompt_template_for_Qwen,user_prompt_template, responseManager);
    // runSurveySimulation_ResidentHttp(
    //     population,
    //     questions,
    //     system_prompt_template_for_Qwen,
    //     user_prompt_template,
    //     responseManager
    // );
    // responseManager.printSummary();
    // exportResultsToFiles(responseManager,merged_population,questions,
    //                        "../../results/individual_responses_bfi2_"+ model_name +".csv",
    //                        "../../data/merged_population_responses_bfi2_" + model_name +".csv");
    //
    // // 集計と初期MAE計算
    // std::vector<std::string> q_ids;
    // for(const auto& q : questions) q_ids.push_back(q.id);
    //
    //
    // // std::string mergerd_csv_path = "../../data/merged_population_responses_bfi2_gemma-3-12b-it.csv";
    // // loadResponsesFromMergedCSV(mergerd_csv_path, responseManager, questions);
    // // std::vector<Person> mae_population;
    // // mae_population = readPopulationFromMergedCSV(mergerd_csv_path);
    // //
    // //
    // if (optManager.loadRealData("../../data/real_ratios.csv") == false) {
    //     std::cerr << "Error: Could not load real data for MAE calculation." << std::endl;
    //     return 1;
    // }
    // optManager.initializeCounts(responseManager, merged_population.size(), q_ids);
    // //double mae = optManager.getCurrentTotalMAE();
    // double mae = optManager.getCurrentTotalMAE();
    // std::cout << mae << std::endl;
    // //初期MAEをファイルに書き出し
    // std::ofstream mae_file("../../log/initial_mae_" + model_name + ".txt");
    // if (mae_file.is_open()) {
    //     mae_file << "Initial MAE: " << mae << std::endl;
    //     mae_file.close();
    // } else {
    //     std::cerr << "Error: Could not open MAE log file." << std::endl;
    // }



    // コンフィグ設定
    ExperimentConfig config;
    config.max_iterations = 403 * 1;       // 試行回数 （適宜調整）
    //config.initial_temperature = 0.02019;   // 初期温度（適宜調整）
    config.initial_temperature = 0.02034;   // 初期温度（適宜調整）
    //config.final_temperature = 0.002252;    // 終了温度（適宜調節）
    config.final_temperature = 0.002269;    // 終了温度（適宜調節）
    //config.cooling_rate = 0.99946;         // 冷却率 （適宜調整）
    config.cooling_rate = std::pow(config.final_temperature/config.initial_temperature, 1.0/config.max_iterations);
    //config.cooling_rate = 0.9998912;
    config.mutation_step_size = 0.05;   // 変化幅
    config.real_data_path = "../../data/real_ratios.csv"; // 作成した正解データ
    config.log_file_path = "../../log/sa_optimization.csv";

    // //実験開始
    // runOptimizationExperiment(
    //     population,
    //     questions,
    //     system_prompt_templates_forQwen,
    //     user_prompt_template,
    //     config
    // );

    // runOptimizationExperimentParallelHttp(
    //     population,
    //     questions,
    //     system_prompt_templates_forQwen,
    //     user_prompt_template,
    //     config
    // );


    // if (!population.empty()) {
    //     std::cout << "!!! TEST MODE: Resizing population to 1 for HTTP test !!!" << std::endl;
    //     population.resize(1);
    // }
    //
    // std::cout << "Starting HTTP Simulation..." << std::endl;
    //
    // runSurveySimulation_ResidentHttp(
    //     population,
    //     questions,
    //     system_prompt_template_for_Qwen,
    //     user_prompt_template,
    //     responseManager
    // );



    return 0;
}