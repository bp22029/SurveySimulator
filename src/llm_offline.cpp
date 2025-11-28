//
// Created by bp22029 on 2025/11/24.
//

#include "llm_offline.hpp"
#include "../include/simulation_runner.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
#include <regex>
#include "nlohmann/json.hpp"
#include <string>
namespace fs = std::filesystem;
using json = nlohmann::json;
#include "../include/prompt_generator.hpp"
#include "individual_response_manager.hpp"
#include "response_parser.hpp"

// src/simulation_runner.cpp の上の方（include文の後、run...関数の前あたり）に追加

// ファイルパス定数（環境に合わせて調整してください）
const std::string BRIDGE_DIR = "/home/bp22029/vllm_bridge";
const std::string REQUEST_FILE = BRIDGE_DIR + "/bridge_request.json";
const std::string RESPONSE_FILE = BRIDGE_DIR + "/bridge_response.json";

// ★今回追加する関数
// 1人のエージェントに対する全質問をまとめて処理する
void sendRequestAndReceiveResponse(
    const Person& person,
    const std::vector<Question>& questions,
    const std::string& system_prompt_template,
    const std::string& user_prompt_template,
    IndividualResponseManager& responseManager,
    std::ofstream* log_file // ログ出力しない場合は nullptr
) {
    // --- 1. リクエストデータの作成 (52問分) ---
    json requests = json::array();

    for (const auto& q : questions) {
        // プロンプト生成
        std::string sys_prompt = generatePrompt(system_prompt_template, person, q);
        std::string usr_prompt = generatePrompt(user_prompt_template, person, q);

        // ID生成: "personID_questionID"
        // (後で結果を紐付けるために必要)
        std::string id = std::to_string(person.person_id) + "_" + q.id;

        // Python側でチャットテンプレートを適用させるために分けて渡す
        requests.push_back({
            {"id", id},
            {"system_prompt", sys_prompt},
            {"user_prompt", usr_prompt}
        });
    }

    // --- 2. リクエストファイルの書き出し (アトミック更新) ---
    // 前回の結果ファイルがもし残っていたら削除
    if (fs::exists(RESPONSE_FILE)) {
        fs::remove(RESPONSE_FILE);
    }

    // 一時ファイルに書き込む
    std::string temp_request_path = REQUEST_FILE + ".tmp";
    std::ofstream req_file(temp_request_path);
    if (!req_file.is_open()) {
        std::cerr << "[C++] Error: Cannot open temporary request file." << std::endl;
        return;
    }
    req_file << requests.dump();
    req_file.close();

    // 書き込み完了後に本番ファイル名にリネーム (Pythonが読み込みエラーを起こさないように)
    fs::rename(temp_request_path, REQUEST_FILE);


    // --- 3. 応答待ち (ポーリング) ---
    // Pythonが処理を終えて bridge_response.json を作るのを待つ
    int timeout_counter = 0;
    while (true) {
        if (fs::exists(RESPONSE_FILE)) {
            // 書き込み競合を防ぐため、念のためごくわずかに待つ
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            break;
        }
        // 0.1秒待機
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // (オプション) タイムアウト処理を入れるならここ
        // timeout_counter++;
        // if (timeout_counter > 600) { ... } // 60秒など
    }


    // --- 4. 結果の読み込み ---
    std::ifstream res_file(RESPONSE_FILE);
    json responses;
    try {
        res_file >> responses;
    } catch (const json::parse_error& e) {
        std::cerr << "[C++] JSON Parse Error: " << e.what() << std::endl;
        res_file.close();
        return;
    }
    res_file.close();

    // 処理が終わったのでレスポンスファイルを削除
    fs::remove(RESPONSE_FILE);


    // --- 5. 結果の記録とログ出力 ---
    for (const auto& item : responses) {
        std::string id_str = item["id"];
        std::string full_response = item["response"];

        // IDから person_id と question_id を復元 ("1_dq1" -> 1, "dq1")
        size_t sep_pos = id_str.find('_');
        if (sep_pos == std::string::npos) continue;

        int p_id = std::stoi(id_str.substr(0, sep_pos));
        std::string q_id = id_str.substr(sep_pos + 1);

        // 回答番号の抽出 (xmlタグ <answer>...</answer> から)
        int choice = extractAnswerFromTags(full_response);

        // マネージャーへの記録
        if (choice != -1) {
            responseManager.recordResponse(p_id, q_id, choice);
        } else {
            // パース失敗時のログなど
            // std::cerr << "Parse failed for Agent " << p_id << ", Q: " << q_id << std::endl;
        }

        // ログファイルへの書き込み（ポインタが有効な場合のみ）
        if (log_file && log_file->is_open()) {
            *log_file << "--------------------------------------------------\n";
            *log_file << "Agent ID: " << p_id << " | Question ID: " << q_id << "\n";
            // モデル名などは固定か引数で渡す
            *log_file << "Model: google/gemma-3-12b-it | Seed: 42 | Temp: 0\n";

            // 思考部分の抽出
            std::string thinking = extractThinkingLog(full_response);
            *log_file << "\n[Reasoning Content]\n" << thinking << "\n";

            int answer = extractAnswerFromTags(full_response);
            *log_file << "\n[Final Answer]\n" << answer << "\n";
            *log_file << "\n";

            // 頻繁なフラッシュは遅くなるので、適宜調整してください
            // log_file->flush();
        }
    }
}