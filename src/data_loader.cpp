//
// Created by bp22029 on 2025/08/12.
//
#include "../include/data_loader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <random>
#include <chrono>

void randomBigFive(Person& person) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    person.personality.neuroticism = dis(gen);
    person.personality.conscientiousness = dis(gen);
    person.personality.extraversion = dis(gen);
    person.personality.agreeableness = dis(gen);
    person.personality.openness = dis(gen);
}

std::vector<Person> readSyntheticPopulation(const std::string& filename) {
    std::vector<Person> population;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "ファイルを開けません: " << filename << std::endl;
        return population;
    }
    std::string line;
    bool isFirstLine = true;
    while (std::getline(file, line)) {
        if (isFirstLine) {
            isFirstLine = false;
            continue;
        }
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        if (fields.size() >= 15) {
            Person person;
            std::string cleaned_id = unquoteString(fields[13]);
            person.person_id = std::stoi(cleaned_id);
            std::string cleaned_gender = unquoteString(fields[16]);
            person.gender = cleaned_gender;
            std::string cleaned_prefecture_name = unquoteString(fields[1]);
            person.prefecture_name = cleaned_prefecture_name;
            std::string cleaned_city_name = unquoteString(fields[3]);
            person.city_name = cleaned_city_name;
            std::string cleaned_age = unquoteString(fields[14]);
            person.age = std::stoi(cleaned_age);
            std::string cleaned_industry_type = unquoteString(fields[20]);
            person.industry_type = cleaned_industry_type;
            std::string cleaned_family_type = unquoteString(fields[10]);
            person.family_type = cleaned_family_type;
            std::string cleaned_role_household_type = unquoteString(fields[18]);
            person.role_household_type = cleaned_role_household_type;
            std::string cleaned_employment_type = unquoteString(fields[22]);
            person.employment_type = cleaned_employment_type;
            std::string cleaned_company_size = unquoteString(fields[24]);
            person.company_size = cleaned_company_size;
            std::string cleaned_income = unquoteString(fields[25]);
            if (fields.size() > 25 && !cleaned_income.empty()) {
                person.total_income = std::stoi(cleaned_income);
            } else {
                person.total_income = 0;
            }
            randomBigFive(person);
            population.push_back(person);
        }
    }

    file.close();
    return population;
}

std::vector<Question> readQuestions(const std::string& filename) {
    std::vector<Question> questions;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "ファイルを開けません: " << filename << std::endl;
        return questions;
    }

    std::string line;
    bool isFirstLine = true;

    while (std::getline(file, line)) {
        if (isFirstLine) {
            isFirstLine = false;
            continue;
        }

        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;

        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() >= 3) {
            Question question;
            question.id = fields[0];
            question.text = fields[1];

            for (size_t i = 2; i < fields.size() && !fields[i].empty(); ++i) {
                question.choices.push_back(fields[i]);
            }

            questions.push_back(question);
        }
    }

    file.close();
    return questions;
}

std::string unquoteString(const std::string& input_str) {
    // 文字列の長さが2文字以上で、かつ先頭と末尾が '"' であるかを確認
    if (input_str.length() >= 2 && input_str.front() == '"' && input_str.back() == '"') {
        // 先頭と末尾の文字を除いた部分文字列を返す
        return input_str.substr(1, input_str.length() - 2);
    }

    // 上の条件に当てはまらない場合は、元の文字列をそのまま返す
    return input_str;
}