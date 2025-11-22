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
#include <map>
#include <filesystem> // C++17以降が必要
namespace fs = std::filesystem;
#include <time.h>

void runSurveySimulation_Parallel(const std::vector<Person>& population,
                                 const std::vector<Question>& questions,
                                 const std::string& prompt_template,
                                 std::vector<SurveyResult>& results,
                                 unsigned int num_threads) { // スレッド数を引数で渡せるようにする


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
    // スレッド数分のワーカースレッドを起動
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_function, std::cref(prompt_template), std::ref(task_queue), std::ref(result_queue),std::cref(servers), std::ref(request_counter));
    }

    // --- 3. メインスレッドで結果を収集・記録 ---
    std::cout << "Waiting for results..." << std::endl;
    for (int i = 0; i < total_simulations; ++i) {
        TaskResult result;
        result_queue.wait_and_pop(result); // 結果が来るまで待機
        //結果の有効性をチェックして記録
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
    responseManager.exportToCSV("../../results/individual_responses.csv", question_ids);
    responseManager.printSummary();
    responseManager.exportMergedPopulationCSV("../../data/merged_population_responses.csv", population, question_ids);

    std::cout << "Simulation finished." << std::endl;
}



// void runSurveySimulation(const std::vector<Person>& population,
//                         const std::vector<Question>& questions,
//                         const std::string& system_prompt_template,
//                         const std::string& user_prompt_template,
//                         std::vector<SurveyResult>& results,
//                         IndividualResponseManager& responseManager,
//                         LlmQueryFunc query_func) {
//
//     int total_simulations = population.size() * questions.size(); //本来はこちら
//     //int total_simulations = 1 * questions.size(); //デバッグ用に最初の1人だけに制限
//     int current_count = 0;
//     std::string generated_system_prompt;
//     std::string generated_user_prompt;
//     //IndividualResponseManager responseManager;
//     // const std::vector<std::pair<std::string, int>> servers = {
//     //     {"127.0.0.1", 8000},
//     //     {"127.0.0.1", 8001}
//     // };
//
//     for (const auto& person : population) {
//         for (int i = 0; i < questions.size(); ++i) {
//             current_count++;
//             std::cout << "\n[" << current_count << "/" << total_simulations << "] "
//                       << "Agent ID: " << person.person_id << ", Question ID: " << questions[i].id << std::endl;
//
//             // プロンプト生成
//             generated_system_prompt = generatePrompt(system_prompt_template, person, questions[i]);
//             generated_user_prompt = generatePrompt(user_prompt_template, person, questions[i]);
//
//
//             //　LLM問い合わせ、質問回答
//             //std::string queryLLM(const std::string& prompt,const std::string& host, int port)
//             // int server_select = current_count % servers.size();
//
//             LLMParams params;
//             params.system_prompt = generated_system_prompt;
//             std::string content;
//             //content = queryLLM(generated_prompt,servers[server_select].first,servers[server_select].second);
//             //content = queryLLM(generated_user_prompt,"127.0.0.1",8000,params);//修正必須
//             content = query_func(generated_user_prompt,"127.0.0.1",8000,params);//修正必須
//             std::cout << content << std::endl;
//
//             // 個人回答の記録
//             //int choice_number = extractChoiceNumber(content);
//             int choice_number = parseLlmAnswer(content);
//             if (choice_number != -1) {
//                 responseManager.recordResponse(person.person_id, questions[i].id, choice_number);
//             }
//             //　回答の解析と集計
//             //parseAndRecordAnswer(content, questions[i], results[i]);
//         }
//         responseManager.printSummary();
//     }
//
//
// //     // 質問IDリストを作成
// //     std::vector<std::string> question_ids;
// //     for (const auto& q : questions) {
// //         question_ids.push_back(q.id);
// //     }
// //     // CSVエクスポート
// //     responseManager.exportToCSV("../../results/individual_responses.csv", question_ids);
// //     responseManager.printSummary();
// //
// //     responseManager.exportMergedPopulationCSV("../../data/merged_population_responses.csv", population, question_ids );
// }

void runSurveySimulation(const std::vector<Person>& population,
                         const std::vector<Question>& questions,
                         const std::string& system_prompt_template,
                         const std::string& user_prompt_template,
                         std::vector<SurveyResult>& results, // 今回は使用していないようですが維持
                         IndividualResponseManager& responseManager,
                         LlmQueryFunc query_func,
                         const std::string& log_filename) { // ★追加: ログファイル名を受け取る

    int total_simulations = population.size() * questions.size();
    int current_count = 0;
    std::string generated_system_prompt;
    std::string generated_user_prompt;

    // -------------------------------------------------------
    // ★追加: ログファイルの作成・オープン
    // -------------------------------------------------------
    std::ofstream log_file(log_filename);
    if (!log_file.is_open()) {
        std::cerr << "Error: Could not open log file " << log_filename << std::endl;
    } else {
        log_file << "Experiment Simulation Log\n";
        log_file << "==================================================\n";
    }

    for (const auto& person : population) {
        for (int i = 0; i < questions.size(); ++i) {
            current_count++;
            std::cout << "\n[" << current_count << "/" << total_simulations << "] "
                      << "Agent ID: " << person.person_id << ", Question ID: " << questions[i].id << std::endl;

            // プロンプト生成
            generated_system_prompt = generatePrompt(system_prompt_template, person, questions[i]);
            generated_user_prompt = generatePrompt(user_prompt_template, person, questions[i]);

            // LLMパラメータ設定
            LLMParams params;
            params.system_prompt = generated_system_prompt;

            // ★変更: 戻り値を string ではなく LLMResponse で受け取る
            LLMResponse result = query_func(generated_user_prompt, "127.0.0.1", 8000, params);

            std::cout << result.content << std::endl; // コンソールには回答のみ表示

            // -------------------------------------------------------
            // ★追加: ログファイルへの書き込み
            // -------------------------------------------------------
            if (log_file.is_open()) {
                log_file << "--------------------------------------------------\n";
                log_file << "Agent ID: " << person.person_id << " | Question ID: " << questions[i].id << "\n";
                log_file << "System Prompt (First 50 chars): " << generated_system_prompt.substr(0, 50) << "...\n"; // 長いので省略表示

                log_file << "\n[Reasoning Content]\n";
                log_file << result.reasoning_content << "\n";

                log_file << "\n[Final Answer]\n";
                log_file << result.content << "\n";
                log_file << "\n";

                log_file.flush(); // 安全のため都度書き込み
            }

            // 個人回答の記録 (result.success のチェック推奨)
            if (result.success) {
                // content ではなく result.content を使用
                int choice_number = parseLlmAnswer(result.content);
                if (choice_number != -1) {
                    responseManager.recordResponse(person.person_id, questions[i].id, choice_number);
                }
            } else {
                 std::cerr << "Error: LLM query failed for Agent " << person.person_id << std::endl;
            }
        }
        // エージェント1人終わるごとにサマリを表示したければここで
        responseManager.printSummary();
    }

    // 最後にファイルを閉じる
    log_file.close();
}

// ★ 引数を、出力したいファイルパス2つに変更
void exportResultsToFiles(const IndividualResponseManager& responseManager,
                               const std::vector<Person>& population,
                               const std::vector<Question>& questions,
                               const std::string& individual_responses_path,
                               const std::string& merged_responses_path) {
    std::vector<std::string> question_ids;
    for (const auto& q : questions) {
        question_ids.push_back(q.id);
    }

    // ★ 引数で渡されたパスを使う
    responseManager.exportToCSV(individual_responses_path, question_ids);
    responseManager.exportMergedPopulationCSV(merged_responses_path, population, question_ids);
}

void runTestSurveySimulation(const std::vector<Person>& population,
                             const std::vector<Question>& questions,
                             const std::map<std::string, std::string>& system_prompt_templates,
                             const std::string& user_prompt_template) {

    if (population.empty() || questions.empty() || system_prompt_templates.empty()) {
        std::cerr << "Error: Invalid input parameters" << std::endl;
        return;
    }

    int total_simulations = population.size() * questions.size() * system_prompt_templates.size();
    int current_count = 0;

    // --- System Prompt Template のループ ---
    for (const auto& [template_name, system_prompt_template] : system_prompt_templates) {
        std::cout << "\n=== Using System Prompt Template: " << template_name << " ===" <<  std::endl;

        // -------------------------------------------------------
        // ログファイルの作成・オープン
        // ファイル名例: "log_bigfive.txt", "log_complex.txt" など
        // 保存先ディレクトリを変えたい場合は "logs/log_" + ... としてください
        // -------------------------------------------------------
        std::string log_dir_path = "../../log/";
        // ディレクトリが存在するか確認し、なければ作成する（エラー回避）
        // 現在日時を取得してファイル名に使う
        if (!fs::exists(log_dir_path)) {
            fs::create_directories(log_dir_path);
        }
        time_t now = time(0);
        tm* ltm = localtime(&now);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", ltm);

        std::string filename = log_dir_path + "log_" + template_name + "_" + timestamp + ".txt";
        std::ofstream log_file(filename);

        if (!log_file.is_open()) {
             std::cerr << "Error: Could not open log file " << filename << std::endl;
             // ログが開けなくても続行するか、returnするかは要件次第ですが、今回は続行します
        } else {
            // ファイルヘッダー書き込み
            log_file << "Simulation Log for Template: " << template_name << "\n";
            log_file << "==================================================\n";
        }

        IndividualResponseManager responseManager;

        for (const auto& person : population) {
            LLMParams params;

            for (int i = 0; i < questions.size(); ++i) {
                current_count++;
                std::cout << "\n[" << current_count << "/" << total_simulations << "] "
                          << "Agent ID: " << person.person_id << ", Question ID: " << questions[i].id << std::endl;

                // プロンプト生成
                std::string generated_system_prompt = generatePrompt(system_prompt_template, person, questions[i]);
                std::string generated_user_prompt = generatePrompt(user_prompt_template, person, questions[i]);

                // パラメータ設定
                params.system_prompt = generated_system_prompt;

                // LLMへの問い合わせ (戻り値が LLMResponse になりました)
                LLMResponse result = queryLLM(generated_user_prompt, "127.0.0.1", 8000, params);

                std::cout << "Answer: " << result.content << std::endl; // コンソールには回答のみ表示

                // -------------------------------------------------------
                // ログファイルへの書き込み
                // -------------------------------------------------------
                if (log_file.is_open()) {
                    log_file << "--------------------------------------------------\n";
                    log_file << "Agent ID: " << person.person_id << " | Question ID: " << questions[i].id << "\n";
                    // parametersの内容もログに記録
                    log_file << "Model: " << params.model << " | Seed: " << params.seed << "\n";
                    log_file << "Temperature: " << params.temperature << "\n";
                    log_file << "Repetition penalty" << params.repetition_penalty << "\n";

                    log_file << "\n[Reasoning Content]\n";
                    log_file << result.reasoning_content << "\n";

                    log_file << "\n[Final Answer]\n";
                    log_file << result.content << "\n";
                    log_file << "\n"; // 空行

                    // バッファをフラッシュして、プログラムが落ちてもログが残るようにする（速度優先なら外してもOK）
                    log_file.flush();
                }

                // 以前の処理の継続
                if (result.success) {
                    // int choice_number = parseLlmAnswer(result.content);
                    int choice_number = extractAnswerFromTags(result.content);
                    if (choice_number != -1) {
                        responseManager.recordResponse(person.person_id, questions[i].id, choice_number);
                    }
                } else {
                    std::cerr << "LLM Query Failed." << std::endl;
                }
            }
        }

        // Summary表示
        responseManager.printSummary();

        // CSV出力
        exportResultsByTemplate(responseManager, questions, template_name);

        // ファイルを閉じる（デストラクタで自動で閉じられますが、明示的に）
        log_file.close();
    }
}

void exportResultsByTemplate(const IndividualResponseManager& responseManager,
                           const std::vector<Question>& questions,
                           const std::string& template_name) {
    std::vector<std::string> question_ids;
    for (const auto& q : questions) {
        question_ids.push_back(q.id);
    }

    time_t now = time(0);
    tm* ltm = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", ltm);
    std::string filename = "../../results/for_test_individual_responses_" + template_name +"_"+ timestamp +  ".csv";
    responseManager.exportToCSV(filename, question_ids);
    responseManager.printSummary();
}