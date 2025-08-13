#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "../include/data_loader.hpp"
#include "../include/prompt_generator.hpp"
#include "llama.h"

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    std::cout << "=== CSVファイル読み込みテスト ===" << std::endl;

    // 1. 合成人口データの読み込みテスト
    std::cout << "\n1. 合成人口データの読み込み..." << std::endl;
    std::vector<Person> population = readSyntheticPopulation("../data/sample_synthetic_population.csv");

    if (population.empty()) {
        std::cout << "エラー: 合成人口データが読み込めませんでした" << std::endl;
    } else {
        std::cout << "読み込み成功: " << population.size() << "人のデータ" << std::endl;

        for (int i = 0; i < std::min(3, static_cast<int>(population.size())); ++i) {
            const Person& p = population[i];
            std::cout << "\n人物" << (i+1) << ":" << std::endl;
            std::cout << "  ID: " << p.id << std::endl;
            std::cout << "  性別: " << p.gender << std::endl;
            std::cout << "  年齢: " << p.age << "歳" << std::endl;
            std::cout << "  住所: " << p.address << std::endl;
            std::cout << "  職業: " << p.industry << std::endl;
            std::cout << "  世帯構成: " << p.household_composition << std::endl;
            std::cout << "  月収: " << p.monthly_income << "円" << std::endl;
            std::cout << "  性格特性:" << std::endl;
            printf("    神経症傾向: %.2f\n", p.personality.neuroticism);
            printf("    誠実性: %.2f\n", p.personality.conscientiousness);
            printf("    外向性: %.2f\n", p.personality.extraversion);
            printf("    協調性: %.2f\n", p.personality.agreeableness);
            printf("    開放性: %.2f\n", p.personality.openness);
        }
    }

    // 2. 質問データの読み込みテスト
    std::cout << "\n\n2. 質問データの読み込み..." << std::endl;
    std::vector<Question> questions = readQuestions("../data/ssm2015.csv");

    if (questions.empty()) {
        std::cout << "エラー: 質問データが読み込めませんでした" << std::endl;
    } else {
        std::cout << "読み込み成功: " << questions.size() << "個の質問" << std::endl;

        for (int i = 0; i < std::min(2, static_cast<int>(questions.size())); ++i) {
            const Question& q = questions[i];
            std::cout << "\n質問" << (i+1) << " [" << q.id << "]:" << std::endl;
            std::cout << q.text << std::endl;
            std::cout << "選択肢:" << std::endl;
            for (const auto& choice : q.choices) {
                std::cout << "  " << choice << std::endl;
            }
        }
    }

    // 3. プロンプトテンプレートの読み込みテスト
    std::cout << "\n\n3. プロンプトテンプレートの読み込み..." << std::endl;
    std::ifstream template_file("../data/prompt_template.txt");
    std::string prompt_template;

    if (template_file.is_open()) {
        std::string line;
        while (std::getline(template_file, line)) {
            prompt_template += line + "\n";
        }
        template_file.close();
        std::cout << "テンプレート読み込み成功" << std::endl;
    } else {
        std::cout << "エラー: プロンプトテンプレートが読み込めませんでした" << std::endl;
    }

    // 4. プロンプト生成テスト
    if (!population.empty() && !questions.empty() && !prompt_template.empty()) {
        std::cout << "\n\n4. プロンプト生成テスト..." << std::endl;

        std::string generated_prompt = generatePrompt(prompt_template, population[0], questions[0]);

        std::cout << "=== 生成されたプロンプト ===" << std::endl;
        std::cout << generated_prompt << std::endl;
        std::cout << "=========================" << std::endl;
    }

    std::cout << "\n=== テスト完了 ===" << std::endl;
    return 0;
}