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


// シミュレーション実行関数
void runSurveySimulation(const std::vector<Person>& population,
                        const std::vector<Question>& questions,
                        const std::string& prompt_template,
                        std::vector<SurveyResult>& results);

void runSurveySimulation_Parallel(const std::vector<Person>& population,
                                 const std::vector<Question>& questions,
                                 const std::string& prompt_template,
                                 std::vector<SurveyResult>& results,
                                 unsigned int num_threads);

void runTestSurveySimulation(const std::vector<Person>& population,
                                const std::vector<Question>& questions,
                                const std::string& prompt_template);

#endif // SIMULATION_RUNNER_HPP