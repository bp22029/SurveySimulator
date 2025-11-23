// individual_responses.hpp
#ifndef THREAD_WORKER_HPP
#define THREAD_WORKER_HPP

#include "../include/prompt_generator.hpp"
#include "response_parser.hpp"
#include "llm_client.hpp"
#include "ThreadSafeQueue.hpp"
#include "person.hpp"   // Person定義が必要
#include "question.hpp" // Question定義が必要
#include <atomic>
#include <vector>
#include <string>


// void worker_function(
//     const std::string& prompt_template,
//     ThreadSafeQueue<SurveyTask>& task_queue,
//     ThreadSafeQueue<TaskResult>& result_queue,
//     const std::vector<std::pair<std::string, int>>& servers, // サーバーリスト
//     std::atomic<unsigned long long>& request_counter // リクエストカウンター
// );

// ワーカースレッドから返ってくる結果（ログ出力用に情報を拡張）
struct ParallelTaskResult {
    int person_id;
    std::string question_id;
    bool success;              // 成功したか
    int choice_number;         // 抽出された回答番号
    std::string system_prompt; // ログ用
    std::string user_prompt;   // ログ用（必要なら）
    LLMResponse llm_response;  // 生のレスポンス（思考プロセス含む）
};

// ワーカー関数の定義
void worker_function(
    const std::string& system_prompt_template,
    const std::string& user_prompt_template,
    ThreadSafeQueue<SurveyTask>& task_queue,
    ThreadSafeQueue<ParallelTaskResult>& result_queue,
    const std::vector<std::pair<std::string, int>>& servers,
    std::atomic<unsigned long long>& request_counter
);

#endif // THREAD_WORKER_HPP