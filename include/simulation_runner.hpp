#ifndef SIMULATION_RUNNER_HPP
#define SIMULATION_RUNNER_HPP

#include <vector>
#include <string>
#include "data_loader.hpp"
#include "survey_results.hpp"
#include <iostream>
#include <fstream>
#include "individual_response_manager.hpp"
#include "person.hpp"
#include "llm_client.hpp"


// シミュレーション実行関数
void runSurveySimulation(const std::vector<Person>& population,
                        const std::vector<Question>& questions,
                        const std::string& system_prompt_template,
                        const std::string& user_prompt_template,
                        std::vector<SurveyResult>& results,
                        IndividualResponseManager& responseManager,
                        LlmQueryFunc query_func);

void runSurveySimulation_Parallel(const std::vector<Person>& population,
                                 const std::vector<Question>& questions,
                                 const std::string& prompt_template,
                                 std::vector<SurveyResult>& results,
                                 unsigned int num_threads);

void runTestSurveySimulation(const std::vector<Person>& population,
                                const std::vector<Question>& questions,
                                const std::string& system_prompt_template,
                                const std::string& user_prompt_template);

void exportResultsToFiles(const IndividualResponseManager& responseManager,
                               const std::vector<Person>& population,
                               const std::vector<Question>& questions,
                               const std::string& individual_responses_path,
                               const std::string& merged_responses_path);

#endif // SIMULATION_RUNNER_HPP