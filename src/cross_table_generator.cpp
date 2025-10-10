//
// Created by bp22029 on 2025/10/06.
//

#include "../include/cross_table_generator.hpp"

CrossGenderTable buildCrossGenderTableFromCsv(const std::string& merged_filename,
                                              const std::vector<Question>& questions) {
    CrossGenderTable cross_gender_table;

    std::ifstream file(merged_filename);
    if (!file.is_open()) {
        std::cerr << "ファイルを開けません: " << merged_filename << std::endl;
        return cross_gender_table;
    }

    std::string line;
    std::vector<std::string> header_list;
    int gender_index = -1;
    std::map<std::string, int> question_indices;

    // ヘッダー解析
    if (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            header_list.push_back(item);
        }
        for (int i = 0; i < static_cast<int>(header_list.size()); ++i) {
            if (header_list[i] == "gender") {
                gender_index = i;
                break;
            }
        }
        for (const auto& q : questions) {
            for (int i = 0; i < static_cast<int>(header_list.size()); ++i) {
                if (header_list[i] == q.id) {
                    question_indices[q.id] = i;
                    break;
                }
            }
        }
    }

    // データ行の集計
    while (std::getline(file, line)) {
        std::vector<std::string> row_data;
        std::istringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            row_data.push_back(item);
        }

        if (gender_index == -1 || row_data.size() <= static_cast<size_t>(gender_index)) {
            continue;
        }
        const std::string& gender_value = row_data[gender_index];
        if (gender_value.empty()) continue;

        for (const auto& q_pair : question_indices) {
            int q_index = q_pair.second;
            if (row_data.size() > static_cast<size_t>(q_index)) {
                const std::string& answer_value = row_data[q_index];
                if (!answer_value.empty()) {
                    cross_gender_table[q_pair.first][gender_value][answer_value]++;
                }
            }
        }
    }

    return cross_gender_table;
}

void printCrossGenderTable(const CrossGenderTable& table) {
    std::cout << "--- クロス集計結果 ---" << std::endl;
    for (const auto& question_pair : table) {
        const std::string& question_id = question_pair.first;
        const auto& gender_map = question_pair.second;
        std::cout << "question_id: [" << question_id << "]" << std::endl;
        for (const auto& gender_pair : gender_map) {
            const std::string& gender = gender_pair.first;
            const auto& answer_map = gender_pair.second;
            std::cout << "  ├─ [" << gender << "]" << std::endl;
            for (const auto& answer_pair : answer_map) {
                const std::string& answer = answer_pair.first;
                int count = answer_pair.second;
                std::cout << "  │   ├─ [" << answer << "] -> " << count << std::endl;
            }
        }
    }
}