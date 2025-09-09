//
// Created by bp22029 on 2025/09/05.
//
#include "../include/simulation_runner.hpp"
#include "../include/prompt_generator.hpp"
#include "individual_response_manager.hpp"
#include "response_parser.hpp"
#include "llm_client.hpp"
#include <iostream>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

void runSurveySimulation(const std::vector<Person>& population,
                        const std::vector<Question>& questions,
                        const std::string& prompt_template,
                        std::vector<SurveyResult>& results) {

    //int total_simulations = population.size() * questions.size(); //本来はこちら
    int total_simulations = 1 * questions.size(); //デバッグ用に最初の1人だけに制限
    int current_count = 0;
    std::string generated_prompt;
    IndividualResponseManager responseManager;

    // for (const auto& person : population) {
    //     for (int i = 0; i < questions.size(); ++i) {
    //         current_count++;
    //         std::cout << "\n[" << current_count << "/" << total_simulations << "] "
    //                   << "Agent ID: " << person.id << ", Question ID: " << questions[i].id << std::endl;
    //
    //         // プロンプト生成
    //         generated_prompt = generatePrompt(prompt_template, person, questions[i]);
    //
    //         //　LLM問い合わせ、質問回答
    //         std::string content = queryLLM(generated_prompt);
    //         std::cout << content << std::endl;
    //
    //         //　回答の解析と集計
    //         //parseAndRecordAnswer(content, questions[i], results[i]);
    //     }
    // }

    //デバッグ用に最初の1人だけに制限
    auto person = population[0];
    for (int i = 0; i < questions.size(); ++i) {
        current_count++;
        std::cout << "\n[" << current_count << "/" << total_simulations << "] "
                  << "Agent ID: " << person.id << ", Question ID: " << questions[i].id << std::endl;

        // プロンプト生成
        generated_prompt = generatePrompt(prompt_template, person, questions[i]);

        //　LLM問い合わせ、質問回答
        std::string content = queryLLM(generated_prompt);
        std::cout << content << std::endl;

        // 個人回答の記録
        int choice_number = extractChoiceNumber(content);
        if (choice_number != -1) {
            responseManager.recordResponse(person.id, questions[i].id, choice_number);
        }

        //　回答の解析と集計
        //parseAndRecordAnswer(content, questions[i], results[i]);


    }

    // 質問IDリストを作成
    std::vector<std::string> question_ids;
    for (const auto& q : questions) {
        question_ids.push_back(q.id);
    }
    // CSVエクスポート
    responseManager.exportToCSV("../data/individual_responses.csv", question_ids);
    responseManager.printSummary();

    // person = population[1]; //デバッグ用に2人目
    // for (int i = 0; i < questions.size(); ++i) {
    //     current_count++;
    //     std::cout << "\n[" << current_count << "/" << total_simulations << "] "
    //               << "Agent ID: " << person.id << ", Question ID: " << questions[i].id << std::endl;
    //
    //     // プロンプト生成
    //     generated_prompt = generatePrompt(prompt_template, person, questions[i]);
    //
    //     //　LLM問い合わせ、質問回答
    //     std::string content = queryLLM(generated_prompt);
    //     std::cout << content << std::endl;
    //
    //     // 個人回答の記録
    //     int choice_number = extractChoiceNumber(content);
    //     if (choice_number != -1) {
    //         responseManager.recordResponse(person.id, questions[i].id, choice_number);
    //     }
    //
    //     //　回答の解析と集計
    //     //parseAndRecordAnswer(content, questions[i], results[i]);
    //
    //     // 質問IDリストを作成
    //     std::vector<std::string> question_ids;
    //     for (const auto& q : questions) {
    //         question_ids.push_back(q.id);
    //     }
    //
    //     // CSVエクスポート
    //     responseManager.exportToCSV("../data/individual_responses.csv", question_ids);
    //     responseManager.printSummary();
    // }
}