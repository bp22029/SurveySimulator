//
// Created by bp22029 on 2025/09/13.
//
#include "../include/thread_worker.hpp"

void worker_function(
    const std::string& prompt_template,
    ThreadSafeQueue<SurveyTask>& task_queue,
    ThreadSafeQueue<TaskResult>& result_queue,
    const std::vector<std::pair<std::string, int>>& servers, // サーバーリスト
    std::atomic<unsigned long long>& request_counter // リクエストカウンター
) {
    SurveyTask task;
    while (task_queue.wait_and_pop(task)) {
        // ラウンドロビンで次にリクエストを送るサーバーを選択
        unsigned long long current_count = request_counter++;
        const auto& target_server = servers[current_count % servers.size()];
        const std::string& host = target_server.first;
        const int port = target_server.second;

        // プロンプト生成
        std::string generated_prompt = generatePrompt(prompt_template, task.person, task.question);

        // ★修正点：引数を追加してqueryLLMを呼び出し
        std::string content = queryLLM(generated_prompt, host, port);

        // 回答の解析と結果のプッシュ
        int choice_number = extractChoiceNumber(content);
        result_queue.push({task.person.person_id, task.question.id, choice_number});
    }
}