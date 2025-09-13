//
// Created by bp22029 on 2025/09/05.
//
#include "../include/simulation_runner.hpp"
#include "../include/prompt_generator.hpp"
#include "individual_response_manager.hpp"
#include "response_parser.hpp"
#include "llm_client.hpp"
#include "ThreadSafeQueue.hpp"
#include "thread_worker.hpp"
#include <thread>
#include <iostream>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

void runSurveySimulation_Parallel(const std::vector<Person>& population,
                                 const std::vector<Question>& questions,
                                 const std::string& prompt_template,
                                 std::vector<SurveyResult>& results,
                                 unsigned int num_threads = 64 ) { // スレッド数を引数で渡せるようにする


    // --- ★追加点：サーバーリストとカウンターを定義 ---
    const std::vector<std::pair<std::string, int>> servers = {
        {"127.0.0.1", 8000},
        {"127.0.0.1", 8001}
    };
    std::atomic<unsigned long long> request_counter(0);

    ThreadSafeQueue<SurveyTask> task_queue;
    ThreadSafeQueue<TaskResult> result_queue;
    IndividualResponseManager responseManager;
    int total_simulations = population.size() * questions.size();

    // --- 1. 全てのタスクをタスクキューに投入 ---
    std::cout << "Generating " << total_simulations << " tasks..." << std::endl;
    for (const auto& person : population) {
        for (const auto& question : questions) {
            task_queue.push({person, question});
        }
    }
    task_queue.notify_finished(); // もうタスクは追加しないことを通知

    // --- 2. ワーカースレッドを起動 ---
    std::cout << "Starting " << num_threads << " worker threads for 2 GPUs..." << std::endl;
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_function, std::cref(prompt_template), std::ref(task_queue), std::ref(result_queue),std::cref(servers), std::ref(request_counter));
    }

    // --- 3. メインスレッドで結果を収集・記録 ---
    std::cout << "Waiting for results..." << std::endl;
    for (int i = 0; i < total_simulations; ++i) {
        TaskResult result;
        result_queue.wait_and_pop(result); // 結果が来るまで待機
        // ★修正点：結果の有効性をここでチェックする
        if (result.choice_number != -1) {
            responseManager.recordResponse(result.person_id, result.question_id, result.choice_number);
        } else {
            // 解析に失敗したタスクをエラー出力
            std::cerr << "Error: Failed to parse response for Person ID: "
                      << result.person_id << ", Question ID: " << result.question_id << std::endl;
        }

        // 進捗表示
        if ((i + 1) % 100 == 0 || (i + 1) == total_simulations) {
            std::cout << "[" << (i + 1) << "/" << total_simulations << "] results processed." << std::endl;
        }
    }

    // --- 4. 全てのワーカースレッドの終了を待つ ---
    std::cout << "Joining threads..." << std::endl;
    for (auto& th : threads) {
        th.join();
    }

    // --- 5. 最終的な結果をエクスポート ---
    std::cout << "Exporting results to CSV..." << std::endl;
    std::vector<std::string> question_ids;
    for (const auto& q : questions) {
        question_ids.push_back(q.id);
    }
    responseManager.exportToCSV("../results/individual_responses.csv", question_ids);
    responseManager.printSummary();
    responseManager.exportMergedPopulationCSV("../data/merged_population_responses.csv", population, question_ids);

    std::cout << "Simulation finished." << std::endl;
}



// void runSurveySimulation(const std::vector<Person>& population,
//                         const std::vector<Question>& questions,
//                         const std::string& prompt_template,
//                         std::vector<SurveyResult>& results) {
//
//     int total_simulations = population.size() * questions.size(); //本来はこちら
//     //int total_simulations = 1 * questions.size(); //デバッグ用に最初の1人だけに制限
//     int current_count = 0;
//     std::string generated_prompt;
//     IndividualResponseManager responseManager;
//
//     for (const auto& person : population) {
//         for (int i = 0; i < questions.size(); ++i) {
//             current_count++;
//             std::cout << "\n[" << current_count << "/" << total_simulations << "] "
//                       << "Agent ID: " << person.person_id << ", Question ID: " << questions[i].id << std::endl;
//
//             // プロンプト生成
//             generated_prompt = generatePrompt(prompt_template, person, questions[i]);
//
//             //　LLM問い合わせ、質問回答
//             std::string content = queryLLM(generated_prompt);
//             std::cout << content << std::endl;
//
//             // 個人回答の記録
//             int choice_number = extractChoiceNumber(content);
//             if (choice_number != -1) {
//                 responseManager.recordResponse(person.person_id, questions[i].id, choice_number);
//             }
//
//             //　回答の解析と集計
//             //parseAndRecordAnswer(content, questions[i], results[i]);
//         }
//     }

    //デバッグ用に最初の1人だけに制限
    // auto person = population[0];
    // for (int i = 0; i < questions.size(); ++i) {
    //     current_count++;
    //     std::cout << "\n[" << current_count << "/" << total_simulations << "] "
    //               << "Agent ID: " << person.person_id << ", Question ID: " << questions[i].id << std::endl;
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
    //         responseManager.recordResponse(person.person_id, questions[i].id, choice_number);
    //     }
    //
    //     //　回答の解析と集計
    //     //parseAndRecordAnswer(content, questions[i], results[i]);
    //
    //
    // }

    // 質問IDリストを作成
    // std::vector<std::string> question_ids;
    // for (const auto& q : questions) {
    //     question_ids.push_back(q.id);
    // }
    // // CSVエクスポート
    // responseManager.exportToCSV("../results/individual_responses.csv", question_ids);
    // responseManager.printSummary();
    //
    // responseManager.exportMergedPopulationCSV("../data/merged_population_responses.csv", population, question_ids );

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
// }