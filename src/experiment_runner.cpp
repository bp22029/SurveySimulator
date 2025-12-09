#include "../include/experiment_runner.hpp"
#include "../include/optimization_manager.hpp"
#include "../include/llm_offline.hpp"
#include "../include/individual_response_manager.hpp"
#include "../include/simulation_runner.hpp" // runSurveySimulation_Resident用
#include <iostream>
#include <fstream>
#include <random>
#include <cmath>
#include <algorithm>
#include <iomanip>

namespace fs = std::filesystem;

// ファイルパス定数（環境に合わせて調整してください）
const std::string BRIDGE_DIR = "/home/bp22029/vllm_bridge";
const std::string REQUEST_FILE = BRIDGE_DIR + "/bridge_request.json";
const std::string RESPONSE_FILE = BRIDGE_DIR + "/bridge_response.json";

// ヘルパー関数: 性格(BigFive)を5つ同時に少し変化させる
Person mutatePerson(const Person& origin, double step_size, std::mt19937& gen) {
    Person mutated = origin;
    // 正規分布に基づいて各特性を変化させる 乱数を作るための型 平均0.0、標準偏差step_size
    std::normal_distribution<> dist_change(0.0, step_size);
    // 0.0～1.0にクランプするラムダ関数
    auto clamp = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };

    // 5つの特性すべてにノイズを加える
    mutated.personality.neuroticism.score     = clamp(mutated.personality.neuroticism.score     + (float)dist_change(gen));
    mutated.personality.conscientiousness.score = clamp(mutated.personality.conscientiousness.score + (float)dist_change(gen));
    mutated.personality.extraversion.score    = clamp(mutated.personality.extraversion.score    + (float)dist_change(gen));
    mutated.personality.agreeableness.score   = clamp(mutated.personality.agreeableness.score   + (float)dist_change(gen));
    mutated.personality.openness.score        = clamp(mutated.personality.openness.score        + (float)dist_change(gen));

    return mutated;
}

void runOptimizationExperiment(
    std::vector<Person>& population,
    const std::vector<Question>& questions,
    const std::map<std::string, std::string>& prompt_templates,
    const std::string& user_prompt_template,
    const ExperimentConfig& config
) {
    std::cout << "=== Optimization Experiment Started ===" << std::endl;

    std::cout << "[Init] Cleaning up residual bridge files..." << std::endl;

    // もし前回のレスポンスが残っていたら消す
    if (fs::exists(RESPONSE_FILE)) {
        fs::remove(RESPONSE_FILE);
        std::cout << "  - Removed old response file." << std::endl;
    }
    // もし前回の送れていないリクエストが残っていたら消す
    if (fs::exists(REQUEST_FILE)) {
        fs::remove(REQUEST_FILE);
        std::cout << "  - Removed old request file." << std::endl;
    }

    // --- 日付付きログファイル名の生成ロジック (ここを追加) ---
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S");
    std::string timestamp = ss.str();

    // 元のパス (例: "../../log/sa.csv") から拡張子を分離して日付を挿入
    std::string final_log_path = config.log_file_path;
    size_t lastindex = final_log_path.find_last_of(".");
    if (lastindex != std::string::npos) {
        // 拡張子がある場合: "sa" + "_20251205..." + ".csv"
        final_log_path = final_log_path.substr(0, lastindex) + "_" + timestamp + final_log_path.substr(lastindex);
    } else {
        // 拡張子がない場合: そのまま後ろにつける
        final_log_path += "_" + timestamp;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_pop(0, population.size() - 1); // 一様分布に基づく個体選択
    std::uniform_real_distribution<> dist_prob(0.0, 1.0);  // 0.0～1.0の一様分布

    std::ofstream main_log(final_log_path);
    //ログファイルのヘッダー　
    main_log << "iteration,temperature,current_mae,result,target_person_id,accepted\n";

    // LLM思考プロセス記録用ログ
    std::string detail_log_path = final_log_path + ".detail.txt";
    std::ofstream detail_log(detail_log_path);

    IndividualResponseManager responseManager;
    OptimizationManager optManager;

    // 正解データの読み込み
    if (!optManager.loadRealData(config.real_data_path)) {
        std::cerr << "[Error] Failed to load real data: " << config.real_data_path << std::endl;
        return;
    }

    // -------------------------------------------------
    // 1. 初期状態の構築 (ベースライン作成)　本番時はコメントアウトを外す
    // -------------------------------------------------
    std::cout << "[Init] Generating baseline responses using runSurveySimulation_Resident..." << std::endl;

    // vLLM用のシステムプロンプト (BigFive用)
    // ※ main.cpp のキー名に合わせてください ("bigfive" や "default" など)
    std::string sys_tmpl = prompt_templates.at("bigfive");
    //
    //
    // runSurveySimulation_Resident(
    //     population,
    //     questions,
    //     sys_tmpl,
    //     user_prompt_template,
    //     responseManager
    // );
    // // タイムスタンプを使ってユニークな名前にします
    // std::string init_csv_path = "../../results/individual_responses_" + timestamp + "_initial.csv";
    // std::string init_merged_path = "../../data/merged_population_" + timestamp + "_initial.csv";
    // std::cout << "[Init] Exporting initial CSVs..." << std::endl;
    // exportResultsToFiles(
    // responseManager,
    // population,
    // questions,
    // init_csv_path,
    // init_merged_path
    // );



    // -------------------------------------------------
    // 1. 初期状態の構築 (CSVから復元して5時間をスキップ！) 本番時はコメントアウト
    // -------------------------------------------------
    std::string initial_csv_path = "../../data/merged_population_responses_BigFive_01.csv"; // ※正しいパスを指定してください
    std::cout << "[Init] Loading baseline from CSV: " << initial_csv_path << std::endl;

    // A. 人口データの復元
    // CSVに記録された「その回答を出した時の性格値」をメモリ上に復元します
    // これをやらないと、メモリ上の性格(ランダム)と回答(CSV)が食い違ってしまいます
    population = readPopulationFromMergedCSV(initial_csv_path);
    if (population.empty()) {
        std::cerr << "[Error] Failed to load population from CSV." << std::endl;
        return;
    }

    // B. 回答データの復元
    // vLLMを回さずに、CSVから回答結果だけをメモリに展開します
    loadResponsesFromMergedCSV(initial_csv_path, responseManager, questions);



    // 集計と初期MAE計算
    std::vector<std::string> q_ids;
    for(const auto& q : questions) q_ids.push_back(q.id);

    optManager.initializeCounts(responseManager, population.size(), q_ids);
    double current_mae = optManager.getCurrentTotalMAE();

    std::cout << "[Init] Done. Initial MAE: " << current_mae << std::endl;






    // -------------------------------------------------
    // 2. SAメインループ
    // -------------------------------------------------
    double temperature = config.initial_temperature;
    std::string checkpoint_path = "../../data/checkpoint_latest.csv"; //バックアップ用パス

    for (int iter = 0; iter < config.max_iterations; ++iter) {
        // A. ターゲット選択と変異
        //int target_idx = dist_pop(gen); //ランダムすぎるため、以下の方法に変更

        // ループの外でインデックスリストを作成
        std::vector<int> agent_indices(population.size());
        std::iota(agent_indices.begin(), agent_indices.end(), 0); // 0, 1, 2... と埋める
        // 最初のシャッフル
        std::shuffle(agent_indices.begin(), agent_indices.end(), gen);
        int current_list_idx = 0; // リストの何番目を見ているか

        for (int iter = 0; iter < config.max_iterations; ++iter) {

            // A. ターゲット選択（リストから順番に取る）
            int target_idx = agent_indices[current_list_idx];
            // 次の人の準備
            current_list_idx++;
            if (current_list_idx >= agent_indices.size()) {
                // 一周しきったら、再度シャッフルして先頭に戻る
                std::shuffle(agent_indices.begin(), agent_indices.end(), gen);
                current_list_idx = 0;
            }


            Person& current_person = population[target_idx];
            Person mutated_person = mutatePerson(current_person, config.mutation_step_size, gen);

            // B. 新しいパラメータで推論 (1人分)
            std::map<std::string, int> new_responses = getResponsesForPerson(
                mutated_person, questions, sys_tmpl, user_prompt_template, &detail_log
            );

            // C. 現在の回答を取得
            IndividualResponse old_res_obj = responseManager.getPersonResponses(current_person.person_id);
            std::map<std::string, int> old_responses = old_res_obj.responses;

            // D. 評価 (Try Update)
            double next_mae = optManager.tryUpdate(old_responses, new_responses);
            double delta = next_mae - current_mae;

            // E. 採用判定 (Metropolis法)
            bool accepted = false;
            if (delta < 0) {
                accepted = true; // 改善
            } else {
                // 改悪でも確率で採用
                double prob = std::exp(-delta / temperature);
                if (dist_prob(gen) < prob) {
                    accepted = true;
                }
            }

            // F. 更新処理
            if (accepted) {
                optManager.commitUpdate(old_responses, new_responses);

                for(auto const& [qid, choice] : new_responses) {
                    responseManager.recordResponse(current_person.person_id, qid, choice);
                }
                // 人口データの更新
                population[target_idx] = mutated_person;
                current_mae = next_mae;


                responseManager.exportMergedPopulationCSV_BigFive(
                    checkpoint_path,
                    population,
                    q_ids
                );
            }

            // G. ログと温度更新
            std::cout << "Iter " << iter << " | T=" << std::fixed << std::setprecision(4) << temperature
                      << " | MAE=" << current_mae
                      << " | " << (accepted ? "[ACC]" : "[REJ]")
                      << " | Agent=" << current_person.person_id
                      << " | d=" << delta << std::endl;

            main_log << iter << "," << temperature << "," << current_mae << ","
                     << (accepted ? "accepted" : "rejected") << ","
                     << current_person.person_id << "," << accepted << "\n";
            main_log.flush();

            temperature *= config.cooling_rate;
        }

        std::cout << "=== Experiment Finished ===" << std::endl;
        std::cout << "Final MAE: " << current_mae << std::endl;

        // 最終的な人口データの保存などをここで行うと良いでしょう
        std::string final_csv_path = "../../results/individual_responses_" + timestamp + "_final.csv";
        std::string final_merged_path = "../../data/merged_population_" + timestamp + "_final.csv";
        std::cout << "[Final] Exporting final CSVs..." << std::endl;
        exportResultsToFiles(
            responseManager,
            population,
            questions,
            final_csv_path,
            final_merged_path
        );
    }
}