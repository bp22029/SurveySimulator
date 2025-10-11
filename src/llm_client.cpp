//
// Created by bp22029 on 2025/08/15.
//

#include "../include/llm_client.hpp"
#include <fstream>
#include <sstream>

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
std::string queryLLM(const std::string& prompt, const std::string& host, int port, const LLMParams& params) {

    // 1. 接続先のサーバーを指定
    httplib::Client cli(host, port);

    // タイムアウトを10分 (600秒) に設定
    cli.set_connection_timeout(60);
    cli.set_read_timeout(600);
    cli.set_write_timeout(60);

    // 2. サーバーに送るリクエスト内容をJSONで作成
    json request_body;
    request_body["model"] = params.model;

    // ベースモデル向けに、システムプロンプトとユーザープロンプトを統合
    std::string combined_prompt = params.system_prompt + "\n" + prompt;

    // messages形式で送信
    request_body["messages"] = json::array({
        {{"role", "user"}, {"content", combined_prompt}}
    });

    // 各種パラメータを設定
    request_body["temperature"] = params.temperature;
    request_body["seed"] = params.seed;
    request_body["stream"] = params.stream;
    request_body["max_tokens"] = params.max_tokens;
    request_body["repetition_penalty"] = params.repetition_penalty;

    // その他のパラメータ（必要であればコメントを外して使用）
    if (params.top_p > 0) {
        request_body["top_p"] = params.top_p;
    }
    // if (!params.stop.empty()) {
    //     request_body["stop"] = params.stop;
    // }

    // 3. "/v1/chat/completions"エンドポイントにPOSTリクエストを送信
    auto res = cli.Post("/v1/chat/completions", request_body.dump(), "application/json");

    // 4. サーバーからの応答を処理
    if (res && res->status == 200) {
        try {
            json response_json = json::parse(res->body);

            // "content" が存在し、かつ null でないことを確認
            if (response_json.contains("choices") && !response_json["choices"].empty() &&
                response_json["choices"][0].contains("message") &&
                response_json["choices"][0]["message"].contains("content") &&
                !response_json["choices"][0]["message"]["content"].is_null())
            {
                return response_json["choices"][0]["message"]["content"];
            }
            else {
                std::cerr << "Warning: LLM response content is null or missing in the response structure." << std::endl;
                std::cerr << "Response Body: " << res->body << std::endl;
                return "エラー：有効な応答がありませんでした。";
            }
        } catch (json::parse_error& e) {
            std::cerr << "Error: Failed to parse JSON response." << std::endl;
            std::cerr << "JSON parser error: " << e.what() << std::endl;
            std::cerr << "Response Body: " << res->body << std::endl;
            return "ERROR";
        }
    } else {
        std::cerr << "Error: Request failed." << std::endl;
        if (res) {
            std::cerr << "Status Code: " << res->status << std::endl;
            std::cerr << "Response Body: " << res->body << std::endl;
        } else {
            auto err = res.error();
            std::cerr << "HTTP Error: " << httplib::to_string(err) << std::endl;
        }
        return "ERROR";
    }
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
