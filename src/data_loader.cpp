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
            person.id = std::stoi(fields[7]);
            person.gender = fields[10];
            person.address = fields[1];
            person.age = std::stoi(fields[8]);
            person.industry = fields[14];
            person.household_composition = fields[4];
            person.household_role = fields[12];

            if (fields.size() > 19 && !fields[19].empty()) {
                person.monthly_income = std::stoi(fields[19]);
            } else {
                person.monthly_income = 0;
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