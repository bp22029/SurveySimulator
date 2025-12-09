#ifndef EXPERIMENT_RUNNER_HPP
#define EXPERIMENT_RUNNER_HPP

#include <vector>
#include <string>
#include <map>
#include "person.hpp"
#include "question.hpp"

// 実験の設定パラメータ
struct ExperimentConfig {
    int max_iterations;       // 最大ループ回数
    double initial_temperature;   // 初期温度
    double cooling_rate;     // 冷却率
    double mutation_step_size ; // 性格値の変化幅（5つ同時に変えるので少し小さめに）
    std::string real_data_path;    // 正解データCSVのパス
    std::string log_file_path;       // ログ出力先
};

// 実験実行関数
void runOptimizationExperiment(
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::map<std::string, std::string>& prompt_templates,
    const std::string& user_prompt_template,
    const ExperimentConfig& config
);

#endif // EXPERIMENT_RUNNER_HPP