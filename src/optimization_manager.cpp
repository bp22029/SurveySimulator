#include "../include/optimization_manager.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

OptimizationManager::OptimizationManager() : population_size(0), current_total_mae(0.0) {}

// 正解データCSVの読み込み (形式: question_id, ratio_0, ratio_1, ratio_2...)
bool OptimizationManager::loadRealData(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open real data CSV: " << filename << std::endl;
        return false;
    }

    std::string line;
    // ヘッダーをスキップするかどうかはCSVの仕様によりますが、ここでは単純な読み込みを実装
    // もしヘッダーがある場合は getline(file, line); を一度呼んでください
    std::getline(file, line); // ヘッダー読み飛ばし

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        std::vector<std::string> row;
        while (std::getline(ss, item, ',')) {
            row.push_back(item);
        }

        if (row.size() < 2) continue;

        std::string q_id = row[0];
        std::vector<double> ratios;
        for (size_t i = 1; i < row.size(); ++i) {
            try {
                if (!row[i].empty()) {
                    ratios.push_back(std::stod(row[i]));
                }
            } catch (...) {
                // 数値変換エラーなどは無視または0扱いに
            }
        }
        real_ratios[q_id] = ratios;

        // カウンターの初期化（サイズ合わせ）
        current_counts[q_id].resize(ratios.size(), 0);
    }
    return true;
}

// ★重要: 選択肢のグルーピングロジック
int OptimizationManager::mapChoiceToBin(const std::string& q_id, int choice) {
    // 選択肢番号そのものが不正な場合は無視（またはエラー用の値を返す）
    if (choice <= 0) return -1;

    // 接頭辞で判定
    if (q_id.find("dq2_") == 0) { // dq2_1 ～ dq2_10
        if (choice <= 2) return 0; // 賛成 (1,2)
        if (choice == 3) return 1; // 中立 (3)
        return 2;                  // 否定 (4,5)
    }
    if (q_id.find("dq3_") == 0) { // dq3_1 ～ dq3_3
        if (choice <= 2) return 0; // 肯定 (1,2)
        return 1;                  // 否定 (3,4) ※Bin数は2
    }
    if (q_id.find("dq4_") == 0) { // dq4_1 ～ dq4_4
        if (choice <= 2) return 0; // 肯定 (1,2)
        return 1;                  // 否定 (3,4)
    }
    if (q_id.find("dq5_") == 0) { // dq5_1 ～ dq5_6
        if (choice <= 2) return 0; // ある (1,2)
        return 1;                  // ない (3,4)
    }
    if (q_id.find("dq6_") == 0) { // dq6_1 ～ dq6_3
        if (choice <= 2) return 0; // 肯定 (1,2)
        if (choice == 3) return 1; // 中立 (3)
        return 2;                  // 否定 (4,5)
    }
    if (q_id.find("dq7_") == 0) { // dq7_1 ～ dq7_4
        if (choice <= 2) return 0; // 高頻度 (1,2)
        return 1;                  // 低頻度 (3,4)
    }
    if (q_id.find("dq8_") == 0) { // dq8_1 ～ dq8_5
        if (choice <= 2) return 0; // 高頻度 (1,2)
        if (choice == 3) return 1; // 中頻度 (3)
        return 2;                  // 低頻度 (4,5)
    }
    if (q_id.find("dq9_") == 0) { // dq9_1 ～ dq9_4
        if (choice <= 3) return 0; // 高頻度 (1,2,3)
        if (choice == 4) return 1; // 中頻度 (4)
        return 2;                  // 低頻度 (5)
    }
    if (q_id.find("dq10_") == 0) { // dq10_1 ～ dq10_2
        if (choice <= 2) return 0; // 肯定 (1,2)
        return 1;                  // 否定 (3,4)
    }
    if (q_id == "dq16") {
        if (choice == 1) return 0; // 高齢世代
        if (choice == 2) return 1; // 両世代
        return 2;                  // 現役世代
    }
    if (q_id.find("dq22_") == 0) { // dq22_1 ～ dq22_6
        if (choice <= 2) return 0; // 頻繁 (1,2)
        if (choice <= 4) return 1; // 時々 (3,4)
        return 2;                  // 皆無 (5)
    }
    if (q_id.find("dq23_") == 0) { // dq23_1 ～ dq23_4
        if (choice <= 2) return 0; // 賛成 (1,2)
        return 1;                  // 否定 (3,4)
    }

    // デフォルト（該当なしの場合）
    return -1;
}

double OptimizationManager::calculateMaeForQuestion(const std::string& question_id) {
    // 正解データがない場合は誤差0として返す
    if (real_ratios.find(question_id) == real_ratios.end()) return 0.0;

    const auto& simulation_counts = current_counts[question_id];

    const auto& real_target_ratios = real_ratios[question_id];

    double sum_of_bin_error = 0.0; // mae -> total_error

    // グループ（Bin）ごとに誤差を計算して合計する
    for (size_t i = 0; i < real_target_ratios.size(); ++i) {

        // 人数を割合に変換 (シミュレーション人数 / 全体人数)
        double simulation_ratio = (population_size > 0) ?
                                  (double)simulation_counts[i] / population_size : 0.0;

        // 正解の割合
        double target_ratio = real_target_ratios[i];

        // 差の絶対値を足す
        sum_of_bin_error += std::abs(simulation_ratio - target_ratio);
    }

    return sum_of_bin_error;
}

// c++
void OptimizationManager::initializeCounts(const IndividualResponseManager& responseManager,
                                           int total_population_size, // pop_size -> 分かりやすく変更
                                           const std::vector<std::string>& target_question_ids) { // question_ids -> 変更

    population_size = total_population_size;
    current_total_mae = 0.0;

    // カウントのリセット
    for (auto& [question_id, count_vec] : current_counts) {
        std::fill(count_vec.begin(), count_vec.end(), 0);
    }

    // 全員分の回答データを取得
    const auto& all_responses_map = responseManager.getAllPersonResponses();

    // 集計ループ
    for (const auto& [person_id, person_response_data] : all_responses_map) {

        for (const auto& question_id : target_question_ids) {

            // 正解データがあるか確認
            auto real_data_it = real_ratios.find(question_id);
            if (real_data_it == real_ratios.end()) continue; // 正解がない質問はスキップ

            // その人がこの質問に回答しているか確認
            auto answer_it = person_response_data.responses.find(question_id);
            if (answer_it == person_response_data.responses.end()) continue; // 未回答ならスキップ

            // 選択肢(1~5)を集計用グループ(0~2)に変換
            int choice_number = answer_it->second;
            int bin = mapChoiceToBin(question_id, choice_number);

            if (bin < 0) continue; // マッピング不可

            // カウント配列の取得とサイズ調整
            auto& simulation_counts = current_counts[question_id];

            // 正解データのグループ数に合わせてサイズ調整
            // 初期化時にサイズが合っていない可能性があるため0埋め
            size_t required_size = real_data_it->second.size();
            if (simulation_counts.size() != required_size) {
                simulation_counts.resize(required_size, 0);
            }

            // カウントアップ
            if (static_cast<size_t>(bin) < simulation_counts.size()) {
                simulation_counts[bin]++;
            }
        }
    }

    // 初期トータルMAEの算出
    current_total_mae = 0.0;
    for (const auto& question_id : target_question_ids) {
        if (real_ratios.find(question_id) == real_ratios.end()) continue;

        current_total_mae += calculateMaeForQuestion(question_id);
    }
}


// 差分更新の試算（SAの判定用）
// ここでは仮の更新を行い、新しいMAEを計算して返す
// old_responses: 変更前の回答マップ (question_id -> choice)
// new_responses: 変更後の回答マップ
double OptimizationManager::tryUpdate(const std::map<std::string, int>& old_responses,
                                      const std::map<std::string, int>& new_responses) {
    double new_total_mae = current_total_mae;

    for (const auto& [q_id, new_choice] : new_responses) {
        // 正解データにない質問はスキップ
        if (real_ratios.find(q_id) == real_ratios.end()) continue;

        int old_choice = -1;
        if (old_responses.count(q_id)) {
            old_choice = old_responses.at(q_id);
        }

        int old_bin = mapChoiceToBin(q_id, old_choice);
        int new_bin = mapChoiceToBin(q_id, new_choice);

        // 回答が変わっていない、またはBinが変わっていない場合はMAE変化なし
        if (old_bin == new_bin) continue;

        // --- 差分計算 ---
        // 1. この質問の「現在のMAE」を引く
        new_total_mae -= calculateMaeForQuestion(q_id);

        // 2. カウントを仮更新
        if (old_bin != -1) current_counts[q_id][old_bin]--;
        if (new_bin != -1) current_counts[q_id][new_bin]++;

        // 3. この質問の「新しいMAE」を足す
        new_total_mae += calculateMaeForQuestion(q_id);

        // 4. カウントを元に戻す（tryなので）
        if (old_bin != -1) current_counts[q_id][old_bin]++;
        if (new_bin != -1) current_counts[q_id][new_bin]--;
    }

    return new_total_mae;
}

// 変更の確定
void OptimizationManager::commitUpdate(const std::map<std::string, int>& old_responses,
                                       const std::map<std::string, int>& new_responses) {

    for (const auto& [q_id, new_choice] : new_responses) {
        if (real_ratios.find(q_id) == real_ratios.end()) continue;

        int old_choice = -1;
        if (old_responses.count(q_id)) {
            old_choice = old_responses.at(q_id);
        }

        int old_bin = mapChoiceToBin(q_id, old_choice);
        int new_bin = mapChoiceToBin(q_id, new_choice);

        if (old_bin == new_bin) continue;

        // 古いMAEを引く
        current_total_mae -= calculateMaeForQuestion(q_id);

        // カウントを更新
        if (old_bin != -1) current_counts[q_id][old_bin]--;
        if (new_bin != -1) current_counts[q_id][new_bin]++;

        // 新しいMAEを足す
        current_total_mae += calculateMaeForQuestion(q_id);
    }
}