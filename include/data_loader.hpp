#ifndef DATA_LOADER_HPP
#define DATA_LOADER_HPP

#include <vector>
#include <string>
#include "person.hpp"
#include "question.hpp"

// CSVデータ読み込み関数
std::vector<Person> readSyntheticPopulation(const std::string& filename);
std::vector<Question> readQuestions(const std::string& filename);
std::string unquoteString(const std::string& input_str);
std::vector<Person> readPopulationForTest(const std::string& filename);

// Big Five性格特性のランダム生成
void randomBigFive(Person& person);

#endif // DATA_LOADER_HPP