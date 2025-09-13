// individual_responses.hpp
#ifndef THREAD_WORKER_HPP
#define THREAD_WORKER_HPP

#include "../include/prompt_generator.hpp"
#include "response_parser.hpp"
#include "llm_client.hpp"
#include "ThreadSafeQueue.hpp"


void worker_function(
    const std::string& prompt_template,
    ThreadSafeQueue<SurveyTask>& task_queue,
    ThreadSafeQueue<TaskResult>& result_queue,
    const std::vector<std::pair<std::string, int>>& servers, // サーバーリスト
    std::atomic<unsigned long long>& request_counter // リクエストカウンター
);

#endif // THREAD_WORKER_HPP