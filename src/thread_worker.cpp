//
// Created by bp22029 on 2025/09/13.
//
#include "../include/thread_worker.hpp"

#include <iostream>

void worker_function(
    const std::string& prompt_template, // プロンプトテンプレート
    ThreadSafeQueue<SurveyTask>& task_queue, // タスクキュー(To-Doリスト)
    ThreadSafeQueue<TaskResult>& result_queue, // 結果キュー(Doneリスト)
    const std::vector<std::pair<std::string, int>>& servers, // サーバーリスト
    std::atomic<unsigned long long>& request_counter // リクエストカウンター
) {
    SurveyTask task; //1．作業員が持っている仕事依頼書を置くための箱
    // 2. タスクキューから仕事を取り出して処理（仕事がなくなるまで繰り返す）
    while (task_queue.wait_and_pop(task)) {
        // ラウンドロビンで次にリクエストを送るサーバーを選択
        // 3.どのサーバーにリクエストを送るか決める（ロードバランシング）
        unsigned long long current_count = request_counter++;
        const auto& target_server = servers[current_count % servers.size()];
        const std::string& host = target_server.first;
        const int port = target_server.second;

        // 4．プロンプト生成
        std::string generated_prompt = generatePrompt(prompt_template, task.person, task.question);
        //std::cout << "Generated Prompt: " << generated_prompt << std::endl;

        // 5．LLM問い合わせ、質問回答
        std::string content = queryLLM(generated_prompt, host, port);
        std::cout << "回答番号: " << content << std::endl;

        // 6. 回答の解析と結果のプッシュ
        const int choice_number = parseLlmAnswer(content);
        std::cout << "Choice Number: " << choice_number << std::endl;
        result_queue.push({task.person.person_id, task.question.id, choice_number});
    }
}
