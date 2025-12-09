#ifndef OPTIMIZATION_MANAGER_HPP
#define OPTIMIZATION_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "individual_response_manager.hpp"

class OptimizationManager {
private:
    // 正解データ: key=question_id, value=[bin0_ratio, bin1_ratio, ...]
    std::map<std::string, std::vector<double>> real_ratios;

    // 現在のシミュレーション集計: key=question_id, value=[bin0_count, bin1_count, ...]
    std::map<std::string, std::vector<int>> current_counts;

    int population_size;
    double current_total_mae;

    // 質問ごとの選択肢(1~5)をBin(0~2)に変換する関数
    int mapChoiceToBin(const std::string& question_id, int choice);

    // 指定された質問の現在のMAEを計算する内部関数
    double calculateMaeForQuestion(const std::string& question_id);

public:
    OptimizationManager();

    // 正解データCSVを読み込む
    bool loadRealData(const std::string& filename);

    // 全エージェントの初期回答から集計状態を初期化する
    void initializeCounts(const IndividualResponseManager& responseManager, int pop_size, const std::vector<std::string>& question_ids);

    // 現在のトータルMAEを取得
    double getCurrentTotalMAE() const { return current_total_mae; }

    // 差分更新の試算（変更後のMAEを返すだけで、内部状態は更新しない）
    // 採用判定に使う
    double tryUpdate(const std::map<std::string, int>& old_responses,
                     const std::map<std::string, int>& new_responses);

    // 変更の確定（内部状態を更新する）
    // 採用が決まった後に呼ぶ
    void commitUpdate(const std::map<std::string, int>& old_responses,
                      const std::map<std::string, int>& new_responses);
};

#endif // OPTIMIZATION_MANAGER_HPP