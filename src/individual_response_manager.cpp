//
// Created by bp22029 on 2025/09/09.
//
// individual_response_manager.cpp
#include "individual_response_manager.hpp"
#include <iostream>
#include <fstream>

void IndividualResponseManager::recordResponse(int person_id, const std::string& question_id, int choice_number) {
    if (person_responses.find(person_id) == person_responses.end()) {
        person_responses[person_id] = IndividualResponse{person_id, {}};
    }

    person_responses[person_id].responses[question_id] = choice_number;
    std::cout << "記録: Person[" << person_id << "] Question[" << question_id << "] Choice[" << choice_number << "]" << std::endl;
}

IndividualResponse IndividualResponseManager::getPersonResponses(int person_id) const {
    auto it = person_responses.find(person_id);
    if (it != person_responses.end()) {
        return it->second;
    }
    return IndividualResponse{person_id, {}};
}

void IndividualResponseManager::exportToCSV(const std::string& filename, const std::vector<std::string>& question_ids) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "CSVファイルを開けません: " << filename << std::endl;
        return;
    }

    // ヘッダー行
    file << "person_id";
    for (const auto& qid : question_ids) {
        file << "," << qid;
    }
    file << "\n";

    // データ行
    for (const auto& [person_id, response] : person_responses) {
        file << person_id;
        for (const auto& qid : question_ids) {
            file << ",";
            auto it = response.responses.find(qid);
            if (it != response.responses.end()) {
                file << it->second;
            }
            // 回答がない場合は空欄
        }
        file << "\n";
    }

    file.close();
    std::cout << "個人回答データをCSVに出力しました: " << filename << std::endl;
}

void IndividualResponseManager::printSummary() const {
    std::cout << "\n=== 個人回答集計サマリー ===" << std::endl;
    std::cout << "回答者数: " << person_responses.size() << std::endl;

    for (const auto& [person_id, response] : person_responses) {
        std::cout << "Person[" << person_id << "]: " << response.responses.size() << "問回答済み" << std::endl;
    }
}
