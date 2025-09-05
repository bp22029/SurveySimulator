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


std::string queryLLM(const std::string& prompt) {

    // 1. 接続先のサーバーを指定
    // 同じPCでサーバーを動かしているので "localhost" を指定します
    //httplib::Client cli("localhost", 8080);
    //LM Studioのサーバーに接続する場合は、以下のようにIPアドレスとポートを指定します。
    //httplib::Client cli("127.0.0.1", 1234);

    httplib::Client cli("127.0.0.1",8000);

    // タイムアウトを10分 (600秒) に設定。これでほとんどのモデルで大丈夫なはずです。
    cli.set_connection_timeout(60); // 接続タイムアウト
    cli.set_read_timeout(120);       // 読み取りタイムアウト ★これを追加！
    cli.set_write_timeout(60);      // 書き込みタイムアウト ★これを追加！


    // 2. サーバーに送るリクエスト内容をJSONで作成
    json request_body;
    //request_body["model"] = "openai/gpt-oss-20b"; // モデル名は任意です
    //request_body["model"] = "meta-llama/Llama-2-7b-chat-hf"; // モデル名は任意です

    request_body["model"] = "hugging-quants/Meta-Llama-3.1-8B-Instruct-AWQ-INT4";

    request_body["messages"] = json::array({
    {{"role", "system"}, {"content", "あなたは社会調査の回答者です。プロフィールに基づいて自然に回答してください。"}},
    {{"role", "user"}, {"content", prompt}}
    });

    //request_body["temperature"] = 0.2;
    request_body["seed"] = 42; // 乱数シードを固定して、同じプロンプトで同じ応答が得られるようにする
    request_body["stream"] = false; // まずは一括で結果を受け取る

    std::cout << "サーバーにリクエストを送信します..." << std::endl;

    // 3. "/v1/chat/completions"エンドポイントにPOSTリクエストを送信
    auto res = cli.Post("/v1/chat/completions", request_body.dump(), "application/json");

    // 4. サーバーからの応答を処理
    if (res && res->status == 200) {
        std::cout << "サーバーから応答を受け取りました。" << std::endl;

        // レスポンスのJSONデータを解析
        json response_json = json::parse(res->body);

        // 生成されたテキストを取得して表示
        std::string content = response_json["choices"][0]["message"]["content"];

        std::cout << "\n--- AIの応答 ---\n" << std::endl;
        return content;

    } else {
        std::cerr << "エラー: リクエストが失敗しました。" << std::endl;
        if(res) {
            std::cerr << "ステータスコード: " << res->status << std::endl;
            std::cerr << "応答内容: " << res->body << std::endl;
        } else {
            auto err = res.error();
            std::cerr << "HTTPエラー: " << httplib::to_string(err) << std::endl;
        }
        return "ERROR";
    }

}
