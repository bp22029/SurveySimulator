#ifndef DATA_LOADER_HPP
#define DATA_LOADER_HPP

#include <vector>
#include <string>
#include "person.hpp"
#include "question.hpp"
#include "individual_response_manager.hpp" // 追加

// CSVデータ読み込み関数
std::vector<Person> readSyntheticPopulation(const std::string& filename);
std::vector<Question> readQuestions(const std::string& filename);
std::string unquoteString(const std::string& input_str);
std::vector<Person> readPopulationForTest(const std::string& filename);
std::vector<Person> readPopulationFromMergedCSV(const std::string& filename);

void loadResponsesFromMergedCSV(const std::string& filename,
                                IndividualResponseManager& responseManager,
                                const std::vector<Question>& questions);
// Big Five性格特性のランダム生成
void randomBigFive(Person& person);

#endif // DATA_LOADER_HPP