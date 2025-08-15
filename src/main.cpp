#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "../include/data_loader.hpp"
#include "../include/prompt_generator.hpp"
#include <iostream>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "llm_client.hpp"
#include "survey_results.hpp"

// nlohmann/jsonを使いやすくするために名前空間を指定
using json = nlohmann::json;

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    // 合成人口データの読み込み
    std::vector<Person> population = readSyntheticPopulation("../data/sample_synthetic_population.csv");
    if (population.empty()) {
        return 1;
    }

    // 質問データの読み込み
    std::vector<Question> questions = readQuestions("../data/ssm2015.csv");
    if (questions.empty()) {
        return 1;
    }

    // プロンプトテンプレートの読み込み
    std::ifstream template_file("../data/prompt_template.txt");
    std::string prompt_template;
    if (template_file.is_open()) {
        std::string line;
        while (std::getline(template_file, line)) {
            prompt_template += line + "\n";
        }
        template_file.close();
    } else {
        return 1;
    }

    std::string generated_prompt;
    // プロンプト生成
    if (!population.empty() && !questions.empty() && !prompt_template.empty()) {
        generated_prompt = generatePrompt(prompt_template, population[0], questions[0]);
    }


    //std::string content = queryLLM(generated_prompt);
    //std::cout << content << std::endl;

    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);



    return 0;
}