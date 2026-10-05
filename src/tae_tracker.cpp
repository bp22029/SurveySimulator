#include "../include/tae_tracker.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

bool TaeTracker::loadRealData(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open real data CSV: " << filename << std::endl;
        return false;
    }
    std::map<std::string, std::vector<double>> ratios;
    std::string line;
    std::getline(file, line); // ヘッダー
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        std::string item;
        std::vector<std::string> row;
        while (std::getline(ss, item, ',')) row.push_back(item);
        if (row.size() < 2) continue;
        std::vector<double> values;
        for (size_t i = 1; i < row.size(); ++i) {
            if (!row[i].empty()) values.push_back(std::stod(row[i]));
        }
        ratios[row[0]] = values;
    }
    setRealData(ratios);
    return true;
}

void TaeTracker::setRealData(const std::map<std::string, std::vector<double>>& real_ratios) {
    real_ratios_ = real_ratios;
}

void TaeTracker::initialize(const std::map<int, Answers>& all_answers, int population_size,
                            const std::vector<std::string>& question_ids) {
    population_size_ = population_size;
    question_ids_.clear();
    question_index_.clear();
    targets_.clear();
    for (const auto& qid : question_ids) {
        auto it = real_ratios_.find(qid);
        if (it == real_ratios_.end()) continue; // 正解がない質問は使わない
        question_index_[qid] = question_ids_.size();
        question_ids_.push_back(qid);
        targets_.push_back(it->second);
    }

    counts_.assign(question_ids_.size(), {});
    for (size_t qi = 0; qi < question_ids_.size(); ++qi) counts_[qi].assign(targets_[qi].size(), 0);

    for (const auto& [person_id, answers] : all_answers) {
        for (const auto& [qid, choice] : answers) {
            auto it = question_index_.find(qid);
            if (it == question_index_.end()) continue;
            int bin = mapChoiceToBin(qid, choice);
            if (bin >= 0 && static_cast<size_t>(bin) < counts_[it->second].size()) {
                counts_[it->second][bin]++;
            }
        }
    }

    errors_.resize(question_ids_.size());
    for (size_t qi = 0; qi < question_ids_.size(); ++qi) errors_[qi] = questionError(qi, counts_[qi]);
    total_ = sumErrors(errors_);
}

std::vector<TaeTracker::Change> TaeTracker::changes(const Answers& old_answers,
                                                    const Answers& new_answers) const {
    std::vector<Change> result;
    for (const auto& [qid, new_choice] : new_answers) {
        auto it = question_index_.find(qid);
        if (it == question_index_.end()) continue;
        auto old_it = old_answers.find(qid);
        int old_bin = old_it == old_answers.end() ? -1 : mapChoiceToBin(qid, old_it->second);
        int new_bin = mapChoiceToBin(qid, new_choice);
        if (old_bin != new_bin) result.push_back({it->second, old_bin, new_bin});
    }
    return result;
}

double TaeTracker::evaluate(const Answers& old_answers, const Answers& new_answers) const {
    std::vector<double> errors = errors_;
    for (const auto& c : changes(old_answers, new_answers)) {
        std::vector<int> counts = counts_[c.question_index];
        if (c.old_bin >= 0) counts[c.old_bin]--;
        if (c.new_bin >= 0) counts[c.new_bin]++;
        errors[c.question_index] = questionError(c.question_index, counts);
    }
    return sumErrors(errors);
}

void TaeTracker::commit(const Answers& old_answers, const Answers& new_answers) {
    for (const auto& c : changes(old_answers, new_answers)) {
        auto& counts = counts_[c.question_index];
        if (c.old_bin >= 0) counts[c.old_bin]--;
        if (c.new_bin >= 0) counts[c.new_bin]++;
        errors_[c.question_index] = questionError(c.question_index, counts);
    }
    total_ = sumErrors(errors_);
}

double TaeTracker::questionError(size_t qi, const std::vector<int>& counts) const {
    double sum = 0.0;
    const auto& target = targets_[qi];
    for (size_t b = 0; b < target.size(); ++b) {
        double ratio = population_size_ > 0 ? static_cast<double>(counts[b]) / population_size_ : 0.0;
        sum += std::abs(ratio - target[b]);
    }
    return sum;
}

double TaeTracker::sumErrors(const std::vector<double>& errors) const {
    double sum = 0.0;
    for (double e : errors) sum += e;
    return sum;
}

// OptimizationManager::mapChoiceToBin と同じ対応表
int TaeTracker::mapChoiceToBin(const std::string& q_id, int choice) {
    if (choice <= 0) return -1;

    if (q_id.find("dq2_") == 0) {   // 賛成(1,2) / 中立(3) / 否定(4,5)
        if (choice <= 2) return 0;
        if (choice == 3) return 1;
        return 2;
    }
    if (q_id.find("dq3_") == 0) {   // 肯定(1,2) / 否定(3,4)
        return choice <= 2 ? 0 : 1;
    }
    if (q_id.find("dq4_") == 0) {   // 肯定(1,2) / 否定(3,4)
        return choice <= 2 ? 0 : 1;
    }
    if (q_id.find("dq5_") == 0) {   // ある(1,2) / ない(3,4)
        return choice <= 2 ? 0 : 1;
    }
    if (q_id.find("dq6_") == 0) {   // 肯定(1,2) / 中立(3) / 否定(4,5)
        if (choice <= 2) return 0;
        if (choice == 3) return 1;
        return 2;
    }
    if (q_id.find("dq7_") == 0) {   // 高頻度(1,2) / 低頻度(3,4)
        return choice <= 2 ? 0 : 1;
    }
    if (q_id.find("dq8_") == 0) {   // 高(1,2) / 中(3) / 低(4,5)
        if (choice <= 2) return 0;
        if (choice == 3) return 1;
        return 2;
    }
    if (q_id.find("dq9_") == 0) {   // 高(1,2,3) / 中(4) / 低(5)
        if (choice <= 3) return 0;
        if (choice == 4) return 1;
        return 2;
    }
    if (q_id.find("dq10_") == 0) {  // 肯定(1,2) / 否定(3,4)
        return choice <= 2 ? 0 : 1;
    }
    if (q_id.find("dq11_") == 0) {  // 肯定(1,2) / 中立(3) / 否定(4,5)
        if (choice <= 2) return 0;
        if (choice == 3) return 1;
        return 2;
    }
    if (q_id.find("dq22_") == 0) {  // 頻繁(1,2) / 時々(3,4) / 皆無(5)
        if (choice <= 2) return 0;
        if (choice <= 4) return 1;
        return 2;
    }
    if (q_id.find("dq23_") == 0) {  // 賛成(1,2) / 否定(3,4)
        return choice <= 2 ? 0 : 1;
    }
    return -1;
}
