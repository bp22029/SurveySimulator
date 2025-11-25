//
// Created by bp22029 on 2025/11/24.
//

#ifndef SURVEYSIMULATOR_LLM_OFFLINE_HPP
#define SURVEYSIMULATOR_LLM_OFFLINE_HPP

#include "llm_offline.hpp"
#include "../include/simulation_runner.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
#include <regex>
#include "nlohmann/json.hpp"
#include <string>
namespace fs = std::filesystem;
using json = nlohmann::json;
#include "../include/prompt_generator.hpp"
#include "individual_response_manager.hpp"
#include "response_parser.hpp"

void sendRequestAndReceiveResponse(
    const Person& person,
    const std::vector<Question>& questions,
    const std::string& system_prompt_template,
    const std::string& user_prompt_template,
    IndividualResponseManager& responseManager,
    std::ofstream* log_file // ログ出力しない場合は nullptr
);

#endif //SURVEYSIMULATOR_LLM_OFFLINE_HPP