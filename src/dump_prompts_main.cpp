// sweep 0（初期個性）の全員分のプロンプトを JSON に書き出す（推論はしない）
//
// 使い方: DumpPrompts <config.json> <output.json>
// 出力は BatchOptimizer が sweep 0 で投げる requests と同じもの。
// job_server の検証（job_server/tools/verify.py）に使う。

#include <exception>
#include <fstream>
#include <iostream>
#include <random>
#include "nlohmann/json.hpp"
#include "../include/batch_optimizer.hpp"
#include "../include/data_loader.hpp"
#include "../include/prompt_generator.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <config.json> <output.json>" << std::endl;
        return 2;
    }
    try {
        const BatchOptimizerConfig c = loadBatchOptimizerConfig(argv[1]);
        // real_ratios_csv は使わないので、存在しなくてもよい
        BatchOptimizerConfig check = c;
        check.real_ratios_csv = c.questions_csv;
        requireLfInputFiles(check);
        std::vector<Person> population = readSyntheticPopulation(c.population_csv);
        const std::vector<Question> questions = readQuestions(c.questions_csv);
        std::mt19937_64 rng(c.seed);
        assignInitialPersonality(population, rng);
        const auto prompts = buildPrompts(population, questions, readPromptTemplate(c.system_prompt_path),
                                          readPromptTemplate(c.user_prompt_path));

        nlohmann::json requests = nlohmann::json::array();
        for (const auto& p : prompts) {
            requests.push_back({{"id", p.id}, {"system_prompt", p.system_prompt}, {"user_prompt", p.user_prompt}});
        }
        std::ofstream out(argv[2], std::ios::binary);
        out << nlohmann::json{{"requests", requests}}.dump();
        if (!out) throw std::runtime_error(std::string("failed to write ") + argv[2]);
        std::cout << "wrote " << prompts.size() << " prompts (" << population.size() << " persons x "
                  << questions.size() << " questions) to " << argv[2] << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
