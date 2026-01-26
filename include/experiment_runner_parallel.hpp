#ifndef SURVEYSIMULATOR_EXPERIMENT_RUNNER_PARALLEL_HPP
#define SURVEYSIMULATOR_EXPERIMENT_RUNNER_PARALLEL_HPP

#include "../include/experiment_runner.hpp"
#include "../include/optimization_manager.hpp"
#include "../include/llm_offline.hpp"
#include "../include/individual_response_manager.hpp"
#include "../include/person.hpp"
#include <iostream>
#include <fstream>
#include <random>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

Person mutatePerson(const Person& origin, double step_size, std::mt19937& gen);

void optimizationWorker(
    int thread_id,
    const std::string& bridge_dir,
    std::atomic<int>& global_iter,
    int max_iterations,
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::string& sys_tmpl,
    const std::string& user_prompt_template,
    const ExperimentConfig& config,
    IndividualResponseManager& responseManager,
    OptimizationManager& optManager,
    double& current_mae,
    double& temperature,
    std::ofstream& main_log,
    const std::vector<int>& agent_indices,
    const std::vector<std::string>& q_ids
);

void runOptimizationExperimentParallel(
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::map<std::string, std::string>& prompt_templates,
    const std::string& user_prompt_template,
    const ExperimentConfig& config
);

#endif //SURVEYSIMULATOR_EXPERIMENT_RUNNER_PARALLEL_HPP