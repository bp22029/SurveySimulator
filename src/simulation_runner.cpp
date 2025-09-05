//
// Created by bp22029 on 2025/09/05.
//
#include "../include/simulation_runner.hpp"
#include "../include/prompt_generator.hpp"
#include "llm_client.hpp"
#include <iostream>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

void runSurveySimulation(const std::vector<Person>& population,
                        const std::vector<Question>& questions,
                        const std::string& prompt_template,
                        std::vector<SurveyResult>& results) {
    int total_simulations = population.size() * questions.size();
    int current_count = 0;
    std::string generated_prompt;

    for (const auto& person : population) {
        for (int i = 0; i < questions.size(); ++i) {
            current_count++;
            std::cout << "\n[" << current_count << "/" << total_simulations << "] "
                      << "Agent ID: " << person.id << ", Question ID: " << questions[i].id << std::endl;

            // プロンプト生成
            generated_prompt = generatePrompt(prompt_template, person, questions[i]);

            //　LLM問い合わせ、質問回答
            std::string content = queryLLM(generated_prompt);
            std::cout << content << std::endl;

            //　回答の解析と集計
            parseAndRecordAnswer(content, questions[i], results[i]);
        }
    }
}