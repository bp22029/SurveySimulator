#ifndef JOB_CLIENT_HPP
#define JOB_CLIENT_HPP

// job_server（ジョブ方式のバッチ推論サーバー）のクライアント。
// POST も GET もすぐに返る設計なので、タイムアウトを短めにしてリトライする（設計書 §4.2）。

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct HttpResponse {
    long status = 0;      // 0 は通信エラー
    std::string body;
    std::string error;    // 通信エラーの内容
};

// 通信部分。テストでは偽物に差し替える
class HttpTransport {
public:
    virtual ~HttpTransport() = default;
    virtual HttpResponse get(const std::string& url, long timeout_sec) = 0;
    virtual HttpResponse postJson(const std::string& url, const std::string& body, long timeout_sec) = 0;
};

// libcurl による実装。結果は gzip で受け取り、libcurl が展開する
class CurlTransport : public HttpTransport {
public:
    CurlTransport();
    ~CurlTransport() override;
    HttpResponse get(const std::string& url, long timeout_sec) override;
    HttpResponse postJson(const std::string& url, const std::string& body, long timeout_sec) override;
};

struct JobPrompt {
    std::string id;            // "personID_questionID"
    std::string system_prompt;
    std::string user_prompt;
};

struct JobOutput {
    std::string id;
    std::string response;
    std::string finish_reason;
};

struct JobStatus {
    int job_id = 0;
    std::string client_id;
    int sweep = 0;
    std::string status;        // queued | running | done | failed
    std::string error;         // failed のとき
    int n_length = 0;          // done のとき
    std::vector<JobOutput> results; // done のとき
};

struct JobClientOptions {
    long post_timeout_sec = 300;   // 1周分の requests は数十 MB になる
    long get_timeout_sec = 600;    // done の結果は展開後に数百 MB になりうる
    int max_attempts = 5;
    int retry_wait_sec = 10;
};

class JobClient {
public:
    JobClient(std::string base_url, std::shared_ptr<HttpTransport> transport,
              JobClientOptions options = {},
              std::function<void(int)> sleep_sec = nullptr);

    // ジョブを投入して job_id を返す。同じ (client_id, sweep) が既にあれば、サーバーは既存の job_id を返す
    int submit(const std::string& client_id, int sweep, const std::vector<JobPrompt>& prompts);

    // (client_id, sweep) のジョブがあれば job_id を返す（クライアント再起動時の復旧用）
    std::optional<int> find(const std::string& client_id, int sweep);

    JobStatus get(int job_id);

    // サーバー情報（GET /info）の JSON 文字列
    std::string info();

private:
    HttpResponse withRetry(const std::string& what, const std::function<HttpResponse()>& request);

    std::string base_url_;
    std::shared_ptr<HttpTransport> transport_;
    JobClientOptions options_;
    std::function<void(int)> sleep_sec_;
};

#endif // JOB_CLIENT_HPP
