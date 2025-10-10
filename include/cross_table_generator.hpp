//
// Created by bp22029 on 2025/10/06.
//

#ifndef SURVEYSIMULATOR_CROSS_TABLE_GENERATOR_HPP
#define SURVEYSIMULATOR_CROSS_TABLE_GENERATOR_HPP

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include "question.hpp"


using CrossGenderTable = std::map<std::string, std::map<std::string, std::map<std::string, int>>>;

CrossGenderTable buildCrossGenderTableFromCsv(const std::string& merged_filename,
                                              const std::vector<Question>& questions);

void printCrossGenderTable(const CrossGenderTable& table);


#endif //SURVEYSIMULATOR_CROSS_TABLE_GENERATOR_HPP