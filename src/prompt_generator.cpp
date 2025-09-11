//
// Created by bp22029 on 2025/08/12.
//
#include "../include/prompt_generator.hpp"
#include <cstdio>
#include <iostream>

std::string readPromptTemplate() {
    std::ifstream template_file("../data/prompt_template.txt");
    std::string prompt_template;
    if (template_file.is_open()) {
        std::string line;
        while (std::getline(template_file, line)) {
            prompt_template += line + "\n";
        }
        template_file.close();
        return prompt_template;
    } else {
        return "";
    }
}

void replaceAll(std::string& str, const std::string& placeholder, const std::string& value) {
    size_t pos = 0;
    while ((pos = str.find(placeholder, pos)) != std::string::npos) {
        str.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
}

std::string generatePrompt(const std::string& template_str, const Person& person, const Question& question) {
    std::string prompt = template_str;
    char buffer[32];

    replaceAll(prompt, "{性別}", person.gender);
    replaceAll(prompt, "{住所}", person.prefecture_name);
    replaceAll(prompt, "{年齢}", std::to_string(person.age));
    replaceAll(prompt, "{職業分類}", person.industry_type);
    replaceAll(prompt, "{雇用形態}", person.employment_type);
    replaceAll(prompt,"{企業規模}", person.company_size);
    replaceAll(prompt, "{世帯構成}", person.family_type);
    replaceAll(prompt, "{世帯での役割}", person.role_household_type);
    replaceAll(prompt, "{月収}", std::to_string(person.total_income));

    sprintf(buffer, "%.2f", person.personality.neuroticism);
    replaceAll(prompt, "{神経症傾向}", buffer);

    sprintf(buffer, "%.2f", person.personality.conscientiousness);
    replaceAll(prompt, "{誠実性}", buffer);

    sprintf(buffer, "%.2f", person.personality.extraversion);
    replaceAll(prompt, "{外向性}", buffer);

    sprintf(buffer, "%.2f", person.personality.agreeableness);
    replaceAll(prompt, "{協調性}", buffer);

    sprintf(buffer, "%.2f", person.personality.openness);
    replaceAll(prompt, "{開放性}", buffer);

    replaceAll(prompt, "{質問}", question.text);

    std::string choices_str;
    for (size_t i = 0; i < question.choices.size(); ++i) {
        choices_str += std::to_string(i + 1) + ". " + question.choices[i] + "\n";
    }
    replaceAll(prompt, "{回答選択肢}", choices_str);

    return prompt;
}