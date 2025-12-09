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
#include "csv_comparer.hpp"
#include "globals.hpp"
#include <time.h>
#include "experiment_runner.hpp"



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

    //仮の一つのテンプレートを読み込む　本番実験用(BigFiveを採用)
    std::string system_prompt_template = readPromptTemplate(system_template_path_bigfive);

    // ユーザープロンプトのテンプレートの読み込み
    std::string user_template_path = "../../data/prompt_templates/user_prompt_template.txt";
    std::string user_prompt_template = readPromptTemplate(user_template_path);

    //　回答集計の配列を初期化
    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);



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




    // 統合したcsvから人口データを読み込む
    // std::string merged_filename = "../../data/merged_population_responses.csv";
    // std::vector<Person> merged_population = readPopulationFromMergedCSV(merged_filename);
    // if (merged_population.empty()) {
    //     return 1;
    // }

    //verificationReproducibility(test_population,questions,system_prompt_templates,user_prompt_template);

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


    // コンフィグ設定
    ExperimentConfig config;
    config.max_iterations = 10000;       // 試行回数
    config.initial_temperature = 0.05;   // 初期温度（適宜調整）
    config.cooling_rate = 0.9998;         // 冷却率
    config.mutation_step_size = 0.05;   // 変化幅
    config.real_data_path = "../../data/real_ratios.csv"; // 作成した正解データ
    config.log_file_path = "../../log/sa_optimization.csv";

    // 実験開始
    runOptimizationExperiment(
        population,
        questions,
        system_prompt_templates,
        user_prompt_template,
        config
    );



    return 0;
}

