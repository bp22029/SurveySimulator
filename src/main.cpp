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

    std::string p1 = generatePrompt(prompt_template, population[402], questions[0]);
    std::cout << p1 << std::endl;

    //　回答集計の配列を初期化
    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);

    // 4. シミュレーションの実行
    runSurveySimulation(population, questions, prompt_template, results);
    //runSurveySimulation_Parallel(population, questions, prompt_template, results, 64); // 64スレッドで実行


    // 5．csvからクロス集計を行う
    std::string merged_filename = "../data/merged_population_responses.csv";
    std::ifstream file(merged_filename);
    if (!file.is_open()) {
        std::cerr << "ファイルを開けません: " << merged_filename << std::endl;
        return 1;
    }

    std::map<std::string, std::map<std::string, std::map<std::string, int>>> cross_gender_table;

    // --- 修正点1: ヘッダー解析をループの外で行う ---
    std::string line;
    std::vector<std::string> header_list;
    int gender_index = -1;
    // map<質問ID, 列インデックス> を用意
    std::map<std::string, int> question_indices;

    // まずヘッダー行だけを読み込む
    if (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            header_list.push_back(item);
        }

        // genderのインデックスを検索
        for (int i = 0; i < header_list.size(); ++i) {
            if (header_list[i] == "gender") {
                gender_index = i;
                break;
            }
        }

        // 全ての質問IDのインデックスを検索して保存
        for (const auto& question : questions) {
            for (int i = 0; i < header_list.size(); ++i) {
                if (header_list[i] == question.id) {
                    question_indices[question.id] = i;
                    break;
                }
            }
        }
    }

    // --- 修正点2: データ行を1行ずつ読み、全質問を処理する ---
    while (std::getline(file, line)) {
        std::vector<std::string> row_data;
        std::istringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            row_data.push_back(item);
        }

        // genderの値を取得
        if (gender_index == -1 || row_data.size() <= gender_index) {
            continue; // genderがなければこの行はスキップ
        }
        std::string gender_value = row_data[gender_index];
        if (gender_value.empty()) {
            continue; // genderが空ならスキップ
        }

        // 保存しておいた全質問のインデックスをループする
        for (const auto& q_pair : question_indices) {
            const std::string& q_id = q_pair.first;
            int q_index = q_pair.second;

            // 回答の値を取得
            if (row_data.size() > q_index) {
                std::string answer_value = row_data[q_index];
                if (!answer_value.empty()) {
                    // 集計用マップを更新
                    cross_gender_table[q_id][gender_value][answer_value]++;
                }
            }
        }
    }

    // --- 集計結果の表示（変更なし） ---
    std::cout << "--- クロス集計結果 ---" << std::endl;
    for (const auto& question_pair : cross_gender_table) {
        const std::string& question_id = question_pair.first;
        const auto& gender_map = question_pair.second;
        std::cout << "question_id: [" << question_id << "]" << std::endl;
        for (const auto& gender_pair : gender_map) {
            const std::string& gender = gender_pair.first;
            const auto& answer_map = gender_pair.second;
            std::cout << "  ├─ [" << gender << "]" << std::endl;
            for (const auto& answer_pair : answer_map) {
                const std::string& answer = answer_pair.first;
                int count = answer_pair.second;
                std::cout << "  │   ├─ [" << answer << "] -> " << count << std::endl;
            }
        }
    }



    return 0;
}