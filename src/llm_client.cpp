//
// Created by bp22029 on 2025/08/15.
//

#include "../include/llm_client.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <map>

#include "httplib.h"
#include "nlohmann/json.hpp"

// nlohmann/jsonを使いやすくするために名前空間を指定
using json = nlohmann::json;



/**
 * @brief vLLMサーバーにリクエストを送信し、LLMからの応答を取得する関数
 *
 * @param prompt ユーザーからのプロンプト
 * @param host サーバーのホスト名またはIPアドレス
 * @param port サーバーのポート番号
 * @param params LLMの各種パラメータ
 * @return std::string LLMからの応答テキスト。エラーの場合は"ERROR"を返す。
 */
// 戻り値を格納する構造体を定義

int generateSeed(const int person_id, const std::string question_id) {
    // person_idとquestion_idを組み合わせて一意のシードを生成
    int seed;
    if (person_id > 1000) {
        seed = (person_id % 1000) * 100 + get_question_number(question_id);
    }else {
        seed = person_id * 100 + get_question_number(question_id);
    }

    return seed;
}



// queryLLMの戻り値を std::string から LLMResponse に変更
LLMResponse queryLLM(const std::string& prompt, const std::string& host, int port, const LLMParams& params) {
    LLMResponse result; // 結果格納用

    httplib::Client cli(host, port);
    cli.set_connection_timeout(60);
    cli.set_read_timeout(600);
    cli.set_write_timeout(60);

    json request_body;
    request_body["model"] = params.model;
    request_body["messages"] = json::array({
        {{"role", "system"}, {"content", params.system_prompt}},
        {{"role", "user"}, {"content", prompt}}
    });

    request_body["temperature"] = params.temperature;
    request_body["seed"] = params.seed;
    request_body["stream"] = params.stream;
    request_body["max_tokens"] = params.max_tokens;
    //request_body["repetition_penalty"] = params.repetition_penalty;
    //request_body["top_k"] = params.top_k;
    //if (params.top_p > 0.0) request_body["top_p"] = params.top_p;

    auto res = cli.Post("/v1/chat/completions", request_body.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            json response_json = json::parse(res->body);

            // デバッグ表示（必要に応じてコメントアウト）
            // std::cout << response_json << std::endl;

            if (response_json.contains("choices") && !response_json["choices"].empty() &&
                response_json["choices"][0].contains("message")) {

                auto& message = response_json["choices"][0]["message"];

                // content (最終回答) の取得
                if (message.contains("content") && !message["content"].is_null()) {
                    result.content = message["content"];
                    result.success = true;
                } else {
                     std::cerr << "Warning: content is null." << std::endl;
                }

                // reasoning_content (思考プロセス) の取得
                // モデルによっては含まれない場合もあるためチェックする
                if (message.contains("reasoning_content") && !message["reasoning_content"].is_null()) {
                    result.reasoning_content = message["reasoning_content"];
                } else {
                    result.reasoning_content = "(No reasoning content provided)";
                }
            }
        } catch (json::parse_error& e) {
            std::cerr << "Error: Failed to parse JSON response. " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Error: Request failed." << std::endl;
    }

    // 失敗時は content は空文字のまま、success は false
    return result;
}




// std::string queryLLM(const std::string& prompt,const std::string& host, int port) {
//
//     // 1. 接続先のサーバーを指定
//     // 同じPCでサーバーを動かしているので "localhost" を指定します
//     //httplib::Client cli("localhost", 8080);
//     //LM Studioのサーバーに接続する場合は、以下のようにIPアドレスとポートを指定します。
//     //httplib::Client cli("127.0.0.1", 1234);
//
//     //　vLLMサーバーに接続する場合
//     //httplib::Client cli("127.0.0.1",8000);
//
//     //GPU2枚構成のvLLMサーバーに接続する場合
//     httplib::Client cli(host,port);
//
//     // タイムアウトを10分 (600秒) に設定。これでほとんどのモデルで大丈夫なはずです。
//     cli.set_connection_timeout(60); // 接続タイムアウト
//     cli.set_read_timeout(600);       // 読み取りタイムアウト ★これを追加！
//     cli.set_write_timeout(60);      // 書き込みタイムアウト ★これを追加！
//
//
//     // 2. サーバーに送るリクエスト内容をJSONで作成
//     json request_body;
//     request_body["model"] = "openai/gpt-oss-120b";
//     //request_body["model"] = "openai/gpt-oss-20b"; //
//     //request_body["model"] = "meta-llama/Llama-2-7b-chat-hf"; // モデル名は任意です
//
//     //request_body["model"] = "hugging-quants/Meta-Llama-3.1-8B-Instruct-AWQ-INT4";
//
//     request_body["messages"] = json::array({
//     {{"role", "system"}, {"content", "あなたは社会調査の回答者です。最終的な回答を出力してください。"}},
//     {{"role", "user"}, {"content", prompt}}
//     });
//
//     request_body["temperature"] = 0.1;
//     request_body["seed"] = 42; // 乱数シードを固定して、同じプロンプトで同じ応答が得られるようにする
//     request_body["stream"] = false; // まずは一括で結果を受け取る
//     request_body["max_tokens"] = 1024; // 例えば1024トークンに制限
//
//     // ★★★ 繰り返しを抑制するペナルティを追加 ★★★
//     request_body["repetition_penalty"] = 1.2;
//
//     //std::cout << "サーバーにリクエストを送信します..." << std::endl;
//
//     // 3. "/v1/chat/completions"エンドポイントにPOSTリクエストを送信
//     auto res = cli.Post("/v1/chat/completions", request_body.dump(), "application/json");
//
//     // 4. サーバーからの応答を処理
//     if (res && res->status == 200) {
//         //std::cout << "サーバーから応答を受け取りました。" << std::endl;
//
//         // レスポンスのJSONデータを解析
//         json response_json = json::parse(res->body);
//
//         // 生成されたテキストを取得して表示
//         std::string content;
//
//         // "content" が存在し、かつ null でないことを確認
//         if (response_json["choices"][0]["message"].contains("content") &&
//             !response_json["choices"][0]["message"]["content"].is_null())
//         {
//             // 安全な場合にのみ content を取り出す
//             content = response_json["choices"][0]["message"]["content"];
//         }
//         else
//         {
//             // content が null または存在しない場合の代替処理
//             content = "エラー：有効な応答がありませんでした。";
//             std::cerr << "Warning: LLM response content is null or missing." << std::endl;
//         }
//
//         //std::cout << "\n--- AIの応答 ---\n" << std::endl;
//         return content;
//
//     } else {
//         std::cerr << "エラー: リクエストが失敗しました。" << std::endl;
//         if(res) {
//             std::cerr << "ステータスコード: " << res->status << std::endl;
//             std::cerr << "応答内容: " << res->body << std::endl;
//         } else {
//             auto err = res.error();
//             std::cerr << "HTTPエラー: " << httplib::to_string(err) << std::endl;
//         }
//         return "ERROR";
//     }
//
// }


int get_question_number(const std::string& question_id) {
    // 静的なマップを定義して、初回呼び出し時にのみ初期化します
    static const std::map<std::string, int> q_map = {
        {"dq2_1", 1},
        {"dq2_2", 2},
        {"dq2_3", 3},
        {"dq2_4", 4},
        {"dq2_5", 5},
        {"dq2_6", 6},
        {"dq2_7", 7},
        {"dq2_8", 8},
        {"dq2_9", 9},
        {"dq2_10", 10},
        {"dq3_1", 11},
        {"dq3_2", 12},
        {"dq3_3", 13},
        {"dq4_1", 14},
        {"dq4_2", 15},
        {"dq4_3", 16},
        {"dq4_4", 17},
        {"dq5_1", 18},
        {"dq5_2", 19},
        {"dq5_3", 20},
        {"dq5_4", 21},
        {"dq5_5", 22},
        {"dq5_6", 23},
        {"dq6_1", 24},
        {"dq6_2", 25},
        {"dq6_3", 26},
        {"dq7_1", 27},
        {"dq7_2", 28},
        {"dq7_3", 29},
        {"dq7_4", 30},
        {"dq8_1", 31},
        {"dq8_2", 32},
        {"dq8_3", 33},
        {"dq8_4", 34},
        {"dq8_5", 35},
        {"dq9_1", 36},
        {"dq9_2", 37},
        {"dq9_3", 38},
        {"dq9_4", 39},
        {"dq10_1", 40},
        {"dq10_2", 41},
        {"dq16", 42},
        {"dq22_1", 43},
        {"dq22_2", 44},
        {"dq22_3", 45},
        {"dq22_4", 46},
        {"dq22_5", 47},
        {"dq22_6", 48},
        {"dq23_1", 49},
        {"dq23_2", 50},
        {"dq23_3", 51},
        {"dq23_4", 52}
    };

    auto it = q_map.find(question_id);
    if (it != q_map.end()) {
        return it->second;
    } else {
        return -1; // 見つからない場合は-1を返す（または例外を投げるなど）
    }
}