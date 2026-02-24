#include "../include/experiment_runner.hpp"
#include "../include/optimization_manager.hpp"
#include "../include/llm_offline.hpp"
#include "../include/individual_response_manager.hpp"
#include <iostream>
#include <fstream>
#include <random>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

namespace fs = std::filesystem;

// 共有リソース保護用のミューテックス
std::mutex g_opt_mutex;     // MAE計算、温度更新、判定用
std::mutex g_log_mutex;     // main_log (CSV) への書き込み用
std::mutex g_pop_mutex;     // population への書き込み用

const int NUM_WORKERS = 2;

// --- mutatePerson (変更なし) ---
Person mutatePerson(const Person& origin, double step_size, std::mt19937& gen) {
    Person mutated = origin;
    std::normal_distribution<> dist_change(0.0, step_size);
    auto clamp = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };

    mutated.personality.neuroticism.anxiety = clamp(mutated.personality.neuroticism.anxiety + (float)dist_change(gen));
    mutated.personality.neuroticism.depression = clamp(mutated.personality.neuroticism.depression + (float)dist_change(gen));
    mutated.personality.neuroticism.emotional_volatility = clamp(mutated.personality.neuroticism.emotional_volatility + (float)dist_change(gen));
    mutated.personality.conscientiousness.organization = clamp(mutated.personality.conscientiousness.organization + (float)dist_change(gen));
    mutated.personality.conscientiousness.productivity = clamp(mutated.personality.conscientiousness.productivity + (float)dist_change(gen));
    mutated.personality.conscientiousness.responsibility = clamp(mutated.personality.conscientiousness.responsibility + (float)dist_change(gen));
    mutated.personality.extraversion.sociability = clamp(mutated.personality.extraversion.sociability + (float)dist_change(gen));
    mutated.personality.extraversion.assertiveness = clamp(mutated.personality.extraversion.assertiveness + (float)dist_change(gen));
    mutated.personality.extraversion.energy_level = clamp(mutated.personality.extraversion.energy_level + (float)dist_change(gen));
    mutated.personality.agreeableness.compassion = clamp(mutated.personality.agreeableness.compassion + (float)dist_change(gen));
    mutated.personality.agreeableness.respectfulness = clamp(mutated.personality.agreeableness.respectfulness + (float)dist_change(gen));
    mutated.personality.agreeableness.trust = clamp(mutated.personality.agreeableness.trust + (float)dist_change(gen));
    mutated.personality.openness.intellectual_curiosity = clamp(mutated.personality.openness.intellectual_curiosity + (float)dist_change(gen));
    mutated.personality.openness.aesthetic_sensitivity = clamp(mutated.personality.openness.aesthetic_sensitivity + (float)dist_change(gen));
    mutated.personality.openness.creative_imagination = clamp(mutated.personality.openness.creative_imagination + (float)dist_change(gen));

    mutated.personality.neuroticism.updateScore();
    mutated.personality.conscientiousness.updateScore();
    mutated.personality.extraversion.updateScore();
    mutated.personality.agreeableness.updateScore();
    mutated.personality.openness.updateScore();
    return mutated;
}

// スレッドWorker
void optimizationWorker(
    int thread_id,
    const std::string& bridge_dir,
    std::atomic<int>& global_iter,
    int max_iterations,
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::string& sys_tmpl,
    const std::string& user_prompt_template,
    const ExperimentConfig& config,
    IndividualResponseManager& responseManager,
    OptimizationManager& optManager,
    double& current_mae,
    double& temperature,
    std::ofstream& main_log,
    const std::vector<int>& agent_indices,
    const std::vector<std::string>& q_ids
) {
    std::mt19937 gen(42 + thread_id);
    std::uniform_real_distribution<> dist_prob(0.0, 1.0);

    while (true) {
        int iter = global_iter.fetch_add(1);
        if (iter >= max_iterations) break;

        int target_idx = agent_indices[iter % agent_indices.size()];
        Person local_person;
        {
            std::lock_guard<std::mutex> lock(g_pop_mutex);
            local_person = population[target_idx];
        }

        // A. 変異と並列推論 (この部分はロック不要)
        Person mutated_person = mutatePerson(local_person, config.mutation_step_size, gen);
        // detail_log は不要なので nullptr を渡す
        std::map<std::string, int> new_responses = getResponsesForPerson(
            mutated_person, questions, sys_tmpl, user_prompt_template, bridge_dir, nullptr
        );

        if (new_responses.empty()) {
            std::cerr << "[T" << thread_id << "] LLM response empty at iter " << iter << std::endl;
            continue;
        }

        // B. 評価と更新 (ここからは順序と整合性が重要なのでロックをかける)
        {
            std::lock_guard<std::mutex> opt_lock(g_opt_mutex);

            IndividualResponse old_res_obj = responseManager.getPersonResponses(local_person.person_id);
            double next_mae = optManager.tryUpdate(old_res_obj.responses, new_responses);
            double delta = next_mae - current_mae;

            bool accepted = (delta < 0) || (dist_prob(gen) < std::exp(-delta / temperature));

            if (accepted) {
                optManager.commitUpdate(old_res_obj.responses, new_responses);
                for(auto const& [qid, choice] : new_responses) {
                    responseManager.recordResponse(local_person.person_id, qid, choice);
                }
                {
                    std::lock_guard<std::mutex> pop_lock(g_pop_mutex);
                    population[target_idx] = mutated_person;
                }
                current_mae = next_mae;
            }

            // ログ記録 (CSVへの書き込みも保護)
            {
                std::lock_guard<std::mutex> log_lock(g_log_mutex);
                main_log << iter << "," << temperature << "," << current_mae << ","
                         << (accepted ? "accepted" : "rejected") << ","
                         << local_person.person_id << "," << (accepted ? 1 : 0) << "," << delta << "\n";
                main_log.flush();

                std::cout << "Iter " << iter << " (GPU" << thread_id << ") | MAE=" << std::fixed << std::setprecision(5) << current_mae
                          << " | " << (accepted ? "[ACC]" : "[REJ]") << " | d=" << delta << std::endl;
            }
            temperature *= config.cooling_rate;
        }
    }
}

void runOptimizationExperimentParallel(
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::map<std::string, std::string>& prompt_templates,
    const std::string& user_prompt_template,
    const ExperimentConfig& config
) {
    std::cout << "=== Parallel Optimization Started ===" << std::endl;

    const std::vector<std::string> bridge_dirs = {
        "/home/bp22029/vllm_bridge_0",
        "/home/bp22029/vllm_bridge_1"
    };

    // 1. 各ブリッジディレクトリの掃除
    for (const auto& dir : bridge_dirs) {
        if (fs::exists(dir + "/bridge_request.json")) fs::remove(dir + "/bridge_request.json");
        if (fs::exists(dir + "/bridge_response.json")) fs::remove(dir + "/bridge_response.json");
    }

    // 2. ログファイルの準備
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S");
    std::string final_log_path = config.log_file_path;
    size_t dot_idx = final_log_path.find_last_of(".");
    final_log_path = final_log_path.substr(0, dot_idx) + "_" + ss.str() + ".csv";

    std::ofstream main_log(final_log_path);
    main_log << "iteration,temperature,current_mae,result,target_person_id,accepted,delta\n";

    // 3. 初期データの復元 (元の実装を維持)
    IndividualResponseManager responseManager;
    OptimizationManager optManager;
    optManager.loadRealData(config.real_data_path);

    std::string initial_csv_path = "../../data/merged_population_20251231_151241_initial.csv";
    population = readPopulationFromMergedCSV(initial_csv_path);
    loadResponsesFromMergedCSV(initial_csv_path, responseManager, questions);

    // 4. 初期MAE計算
    std::vector<std::string> q_ids;
    for(const auto& q : questions) q_ids.push_back(q.id);
    optManager.initializeCounts(responseManager, population.size(), q_ids);
    double current_mae = optManager.getCurrentTotalMAE();
    double temperature = config.initial_temperature;

    // 5. 並列実行準備
    std::atomic<int> global_iter(0);
    std::vector<int> agent_indices(population.size());
    std::iota(agent_indices.begin(), agent_indices.end(), 0);
    std::shuffle(agent_indices.begin(), agent_indices.end(), std::mt19937(42));

    // 6. スレッド起動
    std::string sys_tmpl = prompt_templates.at("bfi2");
    std::vector<std::thread> workers;
    for (int i = 0; i < 2; ++i) {
        workers.emplace_back(
            optimizationWorker, i, bridge_dirs[i], std::ref(global_iter), config.max_iterations,
            std::ref(population), std::ref(questions), sys_tmpl, std::ref(user_prompt_template),
            std::ref(config), std::ref(responseManager), std::ref(optManager),
            std::ref(current_mae), std::ref(temperature), std::ref(main_log),
            std::ref(agent_indices), std::ref(q_ids)
        );
    }

    for (auto& t : workers) t.join();

    // 7. 最終出力
    std::string timestamp = ss.str();
    exportResultsToFiles(responseManager, population, questions,
                         "../../results/parallel_res_" + timestamp + ".csv",
                         "../../data/parallel_merged_" + timestamp + ".csv");
}


void optimizationWorkerHttp(
    int thread_id,
    std::atomic<int>& global_iter,
    int max_iterations,
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::string& sys_tmpl,
    const std::string& user_prompt_template,
    const ExperimentConfig& config,
    IndividualResponseManager& responseManager,
    OptimizationManager& optManager,
    double& current_mae,
    double& temperature,
    std::ofstream& main_log,
    const std::vector<int>& agent_indices,
    const std::vector<std::string>& q_ids
) {
    // スレッドIDに応じてシードをずらす
    std::mt19937 gen(42 + thread_id);
    std::uniform_real_distribution<> dist_prob(0.0, 1.0);

    // ポート番号の決定ロジック
    // thread_id = 0 -> 8000, thread_id = 1 -> 8001
    int port = 8000 + thread_id;
    std::string host = "127.0.0.1";

    while (true) {
        int iter = global_iter.fetch_add(1);
        if (iter >= max_iterations) break;

        // ターゲット選択（ランダム or 順番）
        int target_idx = agent_indices[iter % agent_indices.size()];
        Person local_person;

        // ロックして個体コピー
        {
            std::lock_guard<std::mutex> lock(g_pop_mutex);
            if (target_idx < population.size()) {
                local_person = population[target_idx];
            } else {
                 // エラーハンドリング
                 continue;
            }
        }

        // A. 変異
        Person mutated_person = mutatePerson(local_person, config.mutation_step_size, gen);

        // ★変更: HTTP通信で推論を実行 (ポート指定版)
        // getResponsesForPersonHttpPort は、指定したホスト:ポートに投げる関数として新規作成・実装が必要です
        std::map<std::string, int> new_responses = getResponsesForPersonHttp(
            mutated_person,
            questions,
            sys_tmpl,
            user_prompt_template,
            host,
            port,
            nullptr // log_file
        );

        if (new_responses.empty()) {
            std::cerr << "[T" << thread_id << "] LLM response empty at iter " << iter << std::endl;
            // エラー時もイテレーションを進めるか、再試行するかはポリシー次第
            continue;
        }

        // B. 評価と更新 (ここからは順序と整合性が重要なのでロックをかける)
        {
            std::lock_guard<std::mutex> opt_lock(g_opt_mutex);

            IndividualResponse old_res_obj = responseManager.getPersonResponses(local_person.person_id);
            double next_mae = optManager.tryUpdate(old_res_obj.responses, new_responses);
            double delta = next_mae - current_mae;

            bool accepted = (delta < 0) || (dist_prob(gen) < std::exp(-delta / temperature));

            if (accepted) {
                optManager.commitUpdate(old_res_obj.responses, new_responses);
                for(auto const& [qid, choice] : new_responses) {
                    responseManager.recordResponse(local_person.person_id, qid, choice);
                }
                {
                    std::lock_guard<std::mutex> pop_lock(g_pop_mutex);
                    population[target_idx] = mutated_person;
                }
                current_mae = next_mae;
            }

            // ログ記録 (CSVへの書き込みも保護)
            {
                std::lock_guard<std::mutex> log_lock(g_log_mutex);
                main_log << iter << "," << temperature << "," << current_mae << ","
                         << (accepted ? "accepted" : "rejected") << ","
                         << local_person.person_id << "," << (accepted ? 1 : 0) << "," << delta << "\n";
                main_log.flush();

                std::cout << "Iter " << iter << " (GPU" << thread_id << ") | MAE=" << std::fixed << std::setprecision(5) << current_mae
                          << " | " << (accepted ? "[ACC]" : "[REJ]") << " | d=" << delta << std::endl;
            }
            temperature *= config.cooling_rate;
        }
    }
}

void runOptimizationExperimentParallelHttp(
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::map<std::string, std::string>& prompt_templates,
    const std::string& user_prompt_template,
    const ExperimentConfig& config
) {
    std::cout << "=== Parallel Optimization Started (HTTP Mode) ===" << std::endl;
    std::cout << "Workers: " << NUM_WORKERS << " threads" << std::endl;

    // ★削除: ファイル掃除の処理 (bridge_dirs関連) は不要

    // 1. ログファイルの準備
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S");

    // config.log_file_path の拡張子前にタイムスタンプを挿入
    std::string final_log_path = config.log_file_path;
    size_t dot_idx = final_log_path.find_last_of(".");
    if (dot_idx != std::string::npos) {
        final_log_path = final_log_path.substr(0, dot_idx) + "_" + ss.str() + ".csv";
    } else {
        final_log_path += "_" + ss.str() + ".csv";
    }

    std::ofstream main_log(final_log_path);
    if (!main_log.is_open()) {
        std::cerr << "Error: Could not open log file " << final_log_path << std::endl;
        return;
    }
    main_log << "iteration,temperature,current_mae,result,target_person_id,accepted,delta\n";

    // 2. 初期データの復元
    IndividualResponseManager responseManager;
    OptimizationManager optManager;

    // 実データ(目標分布)のロード
    optManager.loadRealData(config.real_data_path);

    // 初期状態(populationと回答)のロード
    // ※ パスは環境に合わせて適宜修正してください
    std::string initial_csv_path = "../../data/merged_population_20251231_151241_initial.csv";
    std::cout << "Loading initial data from: " << initial_csv_path << std::endl;

    population = readPopulationFromMergedCSV(initial_csv_path);
    loadResponsesFromMergedCSV(initial_csv_path, responseManager, questions);

    // 3. 初期MAE計算
    std::vector<std::string> q_ids;
    for(const auto& q : questions) q_ids.push_back(q.id);

    optManager.initializeCounts(responseManager, population.size(), q_ids);
    double current_mae = optManager.getCurrentTotalMAE();
    double temperature = config.initial_temperature;

    std::cout << "Initial MAE: " << current_mae << ", Initial Temp: " << temperature << std::endl;

    // 4. 並列実行準備
    std::atomic<int> global_iter(0);

    // エージェントのインデックスをシャッフルして処理順序をランダム化
    // (OptimizationWorker内で、このリストを循環してターゲットを選ぶ)
    std::vector<int> agent_indices(population.size());
    std::iota(agent_indices.begin(), agent_indices.end(), 0);
    std::mt19937 g(42);
    std::shuffle(agent_indices.begin(), agent_indices.end(), g);

    // 5. スレッド起動
    std::string sys_tmpl = prompt_templates.at("bfi2");
    std::vector<std::thread> workers;

    // ★変更: bridge_dirs[i] を渡すのをやめ、i (thread_id) だけを渡す
    // Worker内部で port = 8000 + i を計算して接続する
    for (int i = 0; i < NUM_WORKERS; ++i) {
        workers.emplace_back(
            optimizationWorkerHttp,
            i, // thread_id
            std::ref(global_iter),
            config.max_iterations,
            std::ref(population),
            std::ref(questions),
            sys_tmpl,
            std::ref(user_prompt_template),
            std::ref(config),
            std::ref(responseManager),
            std::ref(optManager),
            std::ref(current_mae),
            std::ref(temperature),
            std::ref(main_log),
            std::ref(agent_indices),
            std::ref(q_ids)
        );
    }

    // 6. 全スレッドの終了待ち
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    std::cout << "All workers finished." << std::endl;

    // 7. 最終出力
    std::string timestamp = ss.str();
    exportResultsToFiles(responseManager, population, questions,
                         "../../results/parallel_res_" + timestamp + ".csv",
                         "../../data/parallel_merged_" + timestamp + ".csv");

    std::cout << "=== Optimization Completed ===" << std::endl;
}