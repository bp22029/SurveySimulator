// //
// // Created by bp22029 on 2025/09/13.
// //
// #include "../include/thread_worker.hpp"
//
// #include <iostream>
//
// void worker_function(
//     const std::string& prompt_template, // プロンプトテンプレート
//     ThreadSafeQueue<SurveyTask>& task_queue, // タスクキュー(To-Doリスト)
//     ThreadSafeQueue<TaskResult>& result_queue, // 結果キュー(Doneリスト)
//     const std::vector<std::pair<std::string, int>>& servers, // サーバーリスト
//     std::atomic<unsigned long long>& request_counter // リクエストカウンター
// ) {
//     SurveyTask task; //1．作業員が持っている仕事依頼書を置くための箱
//     // 2. タスクキューから仕事を取り出して処理（仕事がなくなるまで繰り返す）
//     while (task_queue.wait_and_pop(task)) {
//         // ラウンドロビンで次にリクエストを送るサーバーを選択
//         // 3.どのサーバーにリクエストを送るか決める（ロードバランシング）
//         unsigned long long current_count = request_counter++;
//         const auto& target_server = servers[current_count % servers.size()];
//         const std::string& host = target_server.first;
//         const int port = target_server.second;
//
//         // 4．プロンプト生成
//         std::string generated_prompt = generatePrompt(prompt_template, task.person, task.question);
//         //std::cout << "Generated Prompt: " << generated_prompt << std::endl;
//
//         // 5．LLM問い合わせ、質問回答
//         LLMParams params;
//         LLMResponse result = queryLLM(generated_prompt, host, port, params);
//         //std::string content = queryLLM(generated_prompt, host, port,params);
//         std::cout << "回答番号: " << result.content << std::endl;
//
//         // 6. 回答の解析と結果のプッシュ
//         //const int choice_number = parseLlmAnswer(result.content);
//         const int choice_number = extractAnswerFromTags(result.content);
//         std::cout << "Choice Number: " << choice_number << std::endl;
//         result_queue.push({task.person.person_id, task.question.id, choice_number});
//     }
// }

// src/thread_worker.cpp
#include "../include/thread_worker.hpp"
#include <iostream>

void worker_function(
    const std::string& system_prompt_template,
    const std::string& user_prompt_template,
    ThreadSafeQueue<SurveyTask>& task_queue,
    ThreadSafeQueue<ParallelTaskResult>& result_queue,
    const std::vector<std::pair<std::string, int>>& servers,
    std::atomic<unsigned long long>& request_counter
) {
    SurveyTask task;
    // タスクキューから仕事が取れる間はループし続ける
    while (task_queue.wait_and_pop(task)) {
        // 1. サーバーの選択（ラウンドロビン）
        unsigned long long current_count = request_counter++;
        const auto& target_server = servers[current_count % servers.size()];

        // 2. プロンプト生成
        std::string generated_system_prompt = generatePrompt(system_prompt_template, task.person, task.question);
        std::string generated_user_prompt = generatePrompt(user_prompt_template, task.person, task.question);

        // 3. LLMパラメータ設定
        LLMParams params;
        params.system_prompt = generated_system_prompt;
        // 必要であれば seed, temperature 等もここで設定（あるいは引数で渡す）

        // 4. LLMへ問い合わせ
        // ※ queryLLMがスレッドセーフであることを前提とします（通常は問題ありません）
        LLMResponse llm_result = queryLLM(generated_user_prompt, target_server.first, target_server.second, params);

        // 5. 回答の解析（番号抽出）
        int choice_number = -1;
        if (llm_result.success) {
            // extractAnswerFromTags または parseLlmAnswer を使用
            choice_number = extractAnswerFromTags(llm_result.content);
        }

        // 6. 結果をパッケージングして送信
        ParallelTaskResult result_package;
        result_package.person_id = task.person.person_id;
        result_package.question_id = task.question.id;
        result_package.success = llm_result.success;
        result_package.choice_number = choice_number;
        result_package.system_prompt = generated_system_prompt;
        result_package.user_prompt = generated_user_prompt;
        result_package.llm_response = llm_result;

        // 結果キューにプッシュ
        result_queue.push(result_package);

        // 標準出力に進捗を表示（coutはスレッドセーフではないが、文字化け程度で済むため簡易デバッグ用に）
        // std::cout << "." << std::flush;
    }
}