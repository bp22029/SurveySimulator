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
#include <curl/curl.h>
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


// 1人のエージェントに対する全質問をまとめて処理する
void sendRequestAndReceiveResponse(
    const Person& person,
    const std::vector<Question>& questions,
    const std::string& system_prompt_template,
    const std::string& user_prompt_template,
    IndividualResponseManager& responseManager,
    std::ofstream* log_file // ログ出力しない場合は nullptr
) {
    // --- 1. リクエストデータの作成 (51問分) ---
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


    //ログファイルの準備 (ファイル名は適宜設定)
    // time_t now = time(0);
    // tm* ltm = localtime(&now);
    // char timestamp[20];
    // strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", ltm);
    // std::string response_path = RESPONSE_FILE + "." + timestamp;
    // fs::rename(RESPONSE_FILE, response_path);
    //処理が終わったのでレスポンスファイルを改名


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

            //*log_file << "Model: google/gemma-3-12b-it | Seed: 42 | Temp: 0\n";
            // *log_file << "Model: opneai/gpt-oss-20b | Seed: 42 | Temp: 0\n";
            *log_file << "Model: Qwen/Qwen3-14B | Seed: 42 | Temp: 0\n";
            //*log_file << "Model: Qwen3-30B-A3B | Seed: 42 | Temp: 0\n";
            //*log_file << "Model: Qwen3-30B-A3B-Instruct-2507 | Seed: 42 | Temp: 0\n";
            //*log_file << "microsoft/phi-4| Seed: 42 | Temp: 0\n";
            // *log_file << "deepseek-ai/DeepSeek-R1-Distill-Qwen-14B | Seed: 42 | Temp: 0\n";
            //*log_file << "Model: google/gemma-3-27b-it | Seed: 42 | Temp: 0\n";
            //*log_file << "Model: Qwen/Qwen3-32B | Seed: 42 | Temp: 0\n";
            //*log_file << "Model: mistralai/Mistral-Small-24B-Instruct-2501| Seed: 42 | Temp: 0\n";

            // 思考部分の抽出
            //std::string thinking = extractThinkingLog(full_response);
            std::string thinking = extractThinkLog(full_response);
            *log_file << "\n[Reasoning Content]\n" << thinking << "\n";

            int answer = extractAnswerFromTags(full_response);
            *log_file << "\n[Final Answer]\n" << answer << "\n";
            *log_file << "\n";

            // 頻繁なフラッシュは遅くなるので、適宜調整してください
            // log_file->flush();
        }
    }
}

// std::map<std::string, int> getResponsesForPerson(
//     const Person& person,
//     const std::vector<Question>& questions,
//     const std::string& system_prompt_template,
//     const std::string& user_prompt_template,
//     const std::string& bridge_dir,
//     std::ofstream* log_file
// ) {
//     std::map<std::string, int> responses_map;
//     json requests = json::array();
//
//     // 1. リクエスト作成
//     for (const auto& q : questions) {
//         std::string sys_prompt = generatePrompt(system_prompt_template, person, q);
//         std::string usr_prompt = generatePrompt(user_prompt_template, person, q);
//
//         // IDは "questionID" だけでOK（1人分なので）
//         requests.push_back({
//             {"id", q.id},
//             {"system_prompt", sys_prompt},
//             {"user_prompt", usr_prompt}
//         });
//     }
//
//     // 2. ファイル書き出し (bridge_request.json)
//     // ※競合回避のため、一時ファイル経由での書き込みを推奨
//     std::string temp_request_path = REQUEST_FILE + ".tmp";
//     std::ofstream req_file(temp_request_path);
//     if (!req_file.is_open()) {
//         std::cerr << "Error: Cannot open request file." << std::endl;
//         return responses_map; // 空のマップを返す
//     }
//     req_file << requests.dump();
//     req_file.close();
//     fs::rename(temp_request_path, REQUEST_FILE); // アトミックに移動
//
//     // 3. 応答待ち (ポーリング)
//     // ※無限ループ防止のためタイムアウトを入れるのが安全です
//     int timeout_ms = 300000; // 300秒
//     int waited_ms = 0;
//     while (!fs::exists(RESPONSE_FILE)) {
//         std::this_thread::sleep_for(std::chrono::milliseconds(50));
//         waited_ms += 50;
//         if (waited_ms > timeout_ms) {
//             std::cerr << "Timeout waiting for LLM response." << std::endl;
//             return responses_map;
//         }
//     }
//     // 書き込み完了待ち
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));
//
//     // 4. 結果読み込み
//     std::ifstream res_file(RESPONSE_FILE);
//     json responses_json;
//     try {
//         res_file >> responses_json;
//     } catch (...) {
//         std::cerr << "JSON Parse Error" << std::endl;
//         res_file.close();
//         fs::remove(RESPONSE_FILE); // 壊れたファイルは消す
//         return responses_map;
//     }
//     res_file.close();
//     fs::remove(RESPONSE_FILE); // 読み終わったら削除
//
//     // 5. マップに格納 (ここが重要)
//     for (const auto& item : responses_json) {
//         std::string q_id = item["id"];
//         std::string full_response = item["response"];
//
//         // タグから数字を抽出
//         int choice = extractAnswerFromTags(full_response);
//
//         // 有効な回答ならマップに追加
//         if (choice != -1) {
//             responses_map[q_id] = choice;
//         }
//         // 書き込み量が大量のため、コメントアウト
//         // if (log_file && log_file->is_open()) {
//         //     *log_file << "--------------------------------------------------\n";
//         //     // SAの試行中であることを明示しておくと後で見やすいです
//         //     *log_file << "[Optimization Trial] Agent ID: " << person.person_id << " | Question ID: " << q_id << "\n";
//         //     *log_file << "Model: Qwen/Qwen3-14B | Seed: 42 | Temp: 0\n";
//         //
//         //     // 思考プロセスの抽出と記録
//         //     std::string thinking = extractThinkLog(full_response);
//         //     *log_file << "\n[Reasoning Content]\n" << thinking << "\n";
//         //
//         //     *log_file << "\n[Final Answer]\n" << choice << "\n";
//         //     *log_file << "\n";
//         //
//         //     // SAは高速に回るため、頻繁なflushは遅延の原因になりますが、
//         //     // デバッグ中は flush しておいた方が落ちた時にログが残ります。
//         //     // log_file->flush();
//         // }
//     }
//
//
//     return responses_map; // これが new_responses になる
// }

std::map<std::string, int> getResponsesForPerson(
    const Person& person,
    const std::vector<Question>& questions,
    const std::string& system_prompt_template,
    const std::string& user_prompt_template,
    const std::string& bridge_dir, // この引数を使用します
    std::ofstream* log_file
) {
    std::map<std::string, int> responses_map;
    json requests = json::array();

    // --- ここで引数の bridge_dir を使ってパスを生成します ---
    std::string request_file = bridge_dir + "/bridge_request.json";
    std::string response_file = bridge_dir + "/bridge_response.json";

    // 1. リクエスト作成
    for (const auto& q : questions) {
        std::string sys_prompt = generatePrompt(system_prompt_template, person, q);
        std::string usr_prompt = generatePrompt(user_prompt_template, person, q);

        requests.push_back({
            {"id", q.id},
            {"system_prompt", sys_prompt},
            {"user_prompt", usr_prompt}
        });
    }

    // 2. ファイル書き出し
    // REQUEST_FILE ではなくローカル変数の request_file を使用
    std::string temp_request_path = request_file + ".tmp";
    std::ofstream req_file(temp_request_path);
    if (!req_file.is_open()) {
        std::cerr << "Error: Cannot open request file: " << temp_request_path << std::endl;
        return responses_map;
    }
    req_file << requests.dump();
    req_file.close();
    fs::rename(temp_request_path, request_file); // request_file を使用

    // 3. 応答待ち (ポーリング)
    int timeout_ms = 300000;
    int waited_ms = 0;
    // RESPONSE_FILE ではなくローカル変数の response_file を使用
    while (!fs::exists(response_file)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        waited_ms += 500;
        if (waited_ms > timeout_ms) {
            std::cerr << "Timeout waiting for LLM response in: " << bridge_dir << std::endl;
            return responses_map;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 4. 結果読み込み
    // response_file を使用
    std::ifstream res_file(response_file);
    json responses_json;
    try {
        res_file >> responses_json;
    } catch (...) {
        std::cerr << "JSON Parse Error in: " << response_file << std::endl;
        res_file.close();
        if (fs::exists(response_file)) fs::remove(response_file);
        return responses_map;
    }
    res_file.close();
    if (fs::exists(response_file)) fs::remove(response_file); // 読み終わったら削除

    // 5. マップに格納
    for (const auto& item : responses_json) {
        std::string q_id = item["id"];
        std::string full_response = item["response"];
        int choice = extractAnswerFromTags(full_response);
        if (choice != -1) {
            responses_map[q_id] = choice;
        }
        // 書き込み量が大量のため、コメントアウト
        // if (log_file && log_file->is_open()) {
        //     *log_file << "--------------------------------------------------\n";
        //     // SAの試行中であることを明示しておくと後で見やすいです
        //     *log_file << "[Optimization Trial] Agent ID: " << person.person_id << " | Question ID: " << q_id << "\n";
        //     *log_file << "Model: Qwen/Qwen3-14B | Seed: 42 | Temp: 0\n";
        //
        //     // 思考プロセスの抽出と記録
        //     std::string thinking = extractThinkLog(full_response);
        //     *log_file << "\n[Reasoning Content]\n" << thinking << "\n";
        //
        //     *log_file << "\n[Final Answer]\n" << choice << "\n";
        //     *log_file << "\n";
        //
        //     // SAは高速に回るため、頻繁なflushは遅延の原因になりますが、
        //     // デバッグ中は flush しておいた方が落ちた時にログが残ります。
        //     // log_file->flush();
        // }
    }

    return responses_map;
}

// ==========================================
// HTTP版
// ==========================================

// ヘルパー関数: レスポンス書き込み用コールバック
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

// ヘルパー関数: POST送信
static std::string sendHttpPost(const std::string& url, const std::string& json_data) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // タイムアウト設定 (300秒)
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "[C++] curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    } else {
        std::cerr << "[C++] Error: Failed to init curl." << std::endl;
    }
    return readBuffer;
}

// HTTP版メイン関数
void sendRequestAndReceiveResponseHttp(
    const Person& person,
    const std::vector<Question>& questions,
    const std::string& system_prompt_template,
    const std::string& user_prompt_template,
    IndividualResponseManager& responseManager,
    std::ofstream* log_file
) {
    // 1. リクエストデータの作成 (既存ロジックと同じ)
    json requests = json::array();
    for (const auto& q : questions) {
        std::string sys_prompt = generatePrompt(system_prompt_template, person, q);
        std::string usr_prompt = generatePrompt(user_prompt_template, person, q);
        std::string id = std::to_string(person.person_id) + "_" + q.id;
        requests.push_back({
            {"id", id},
            {"system_prompt", sys_prompt},
            {"user_prompt", usr_prompt}
        });
    }

    // 2. HTTP通信
    // ポート8000は server.py の設定に合わせてください
    std::string url = "http://127.0.0.1:8000/generate";
    std::string request_json_str = requests.dump();
    std::string response_str = sendHttpPost(url, request_json_str);

    if (response_str.empty()) {
        std::cerr << "[C++] Error: Received empty response for Agent " << person.person_id << std::endl;
        return;
    }

    // 3. JSONパース
    json responses;
    try {
        responses = json::parse(response_str);
    } catch (const json::parse_error& e) {
        std::cerr << "[C++] JSON Parse Error: " << e.what() << std::endl;
        return;
    }

    // 4. 結果の記録
    for (const auto& item : responses) {
        std::string id_str = item["id"];
        std::string full_response = item["response"];

        size_t sep_pos = id_str.find('_');
        if (sep_pos == std::string::npos) continue;
        int p_id = std::stoi(id_str.substr(0, sep_pos));
        std::string q_id = id_str.substr(sep_pos + 1);

        int choice = extractAnswerFromTags(full_response);
        if (choice != -1) {
            responseManager.recordResponse(p_id, q_id, choice);
        }

        // ログ出力
        if (log_file && log_file->is_open()) {
            *log_file << "--------------------------------------------------\n";
            *log_file << "Agent ID: " << p_id << " | Question ID: " << q_id << "\n";
            // 思考ログ抽出用の関数などは既存のものを再利用
             std::string thinking = extractThinkLog(full_response);
             *log_file << "\n[Reasoning Content]\n" << thinking << "\n";
            *log_file << "\n[Final Answer]\n" << choice << "\n\n";
            log_file->flush();
        }
    }
}