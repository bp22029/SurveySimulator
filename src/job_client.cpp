#include "../include/job_client.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <curl/curl.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// ==========================================
// CurlTransport
// ==========================================

namespace {

size_t appendToString(void* contents, size_t size, size_t nmemb, void* userp) {
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

HttpResponse perform(CURL* curl, long timeout_sec) {
    HttpResponse res;
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // gzip を受け取り、自動で展開する
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.status);
    } else {
        res.error = curl_easy_strerror(code);
    }
    return res;
}

} // namespace

CurlTransport::CurlTransport() { curl_global_init(CURL_GLOBAL_DEFAULT); }
CurlTransport::~CurlTransport() { curl_global_cleanup(); }

HttpResponse CurlTransport::get(const std::string& url, long timeout_sec) {
    CURL* curl = curl_easy_init();
    if (!curl) return {0, "", "curl_easy_init failed"};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    HttpResponse res = perform(curl, timeout_sec);
    curl_easy_cleanup(curl);
    return res;
}

HttpResponse CurlTransport::postJson(const std::string& url, const std::string& body, long timeout_sec) {
    CURL* curl = curl_easy_init();
    if (!curl) return {0, "", "curl_easy_init failed"};
    curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    HttpResponse res = perform(curl, timeout_sec);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res;
}

// ==========================================
// JobClient
// ==========================================

JobClient::JobClient(std::string base_url, std::shared_ptr<HttpTransport> transport,
                     JobClientOptions options, std::function<void(int)> sleep_sec)
    : base_url_(std::move(base_url)), transport_(std::move(transport)), options_(options),
      sleep_sec_(std::move(sleep_sec)) {
    while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
    if (!sleep_sec_) {
        sleep_sec_ = [](int s) { std::this_thread::sleep_for(std::chrono::seconds(s)); };
    }
}

HttpResponse JobClient::withRetry(const std::string& what, const std::function<HttpResponse()>& request) {
    HttpResponse res;
    for (int attempt = 1; attempt <= options_.max_attempts; ++attempt) {
        res = request();
        // 通信エラーと 5xx だけリトライする。4xx は投げ直しても結果が変わらない
        if (res.status != 0 && res.status < 500) return res;
        std::cerr << "[JobClient] " << what << " failed (attempt " << attempt << "/"
                  << options_.max_attempts << "): "
                  << (res.status == 0 ? res.error : "HTTP " + std::to_string(res.status)) << std::endl;
        if (attempt < options_.max_attempts) sleep_sec_(options_.retry_wait_sec);
    }
    throw std::runtime_error(what + " failed after " + std::to_string(options_.max_attempts) + " attempts");
}

int JobClient::submit(const std::string& client_id, int sweep, const std::vector<JobPrompt>& prompts) {
    json requests = json::array();
    for (const auto& p : prompts) {
        requests.push_back({{"id", p.id}, {"system_prompt", p.system_prompt}, {"user_prompt", p.user_prompt}});
    }
    const std::string body = json{{"client_id", client_id}, {"sweep", sweep}, {"requests", requests}}.dump();

    HttpResponse res = withRetry("POST /jobs", [&] {
        return transport_->postJson(base_url_ + "/jobs", body, options_.post_timeout_sec);
    });
    if (res.status == 409) {
        // 同じ周に別の内容のジョブがある。乱数や設定が前回と食い違っている
        throw std::runtime_error("POST /jobs: a different job already exists for client_id=" + client_id +
                                 " sweep=" + std::to_string(sweep) + ": " + res.body);
    }
    if (res.status != 200) {
        throw std::runtime_error("POST /jobs: HTTP " + std::to_string(res.status) + ": " + res.body.substr(0, 500));
    }
    return json::parse(res.body).at("job_id").get<int>();
}

std::optional<int> JobClient::find(const std::string& client_id, int sweep) {
    // client_id は英数字と記号を想定し、URL エンコードはしない
    const std::string url = base_url_ + "/jobs?client_id=" + client_id + "&sweep=" + std::to_string(sweep);
    HttpResponse res = withRetry("GET /jobs", [&] { return transport_->get(url, options_.get_timeout_sec); });
    if (res.status != 200) {
        throw std::runtime_error("GET /jobs: HTTP " + std::to_string(res.status) + ": " + res.body.substr(0, 500));
    }
    const auto jobs = json::parse(res.body).at("jobs");
    if (jobs.empty()) return std::nullopt;
    return jobs.at(0).at("job_id").get<int>();
}

JobStatus JobClient::get(int job_id) {
    const std::string url = base_url_ + "/jobs/" + std::to_string(job_id);
    HttpResponse res = withRetry("GET /jobs/" + std::to_string(job_id),
                                 [&] { return transport_->get(url, options_.get_timeout_sec); });
    if (res.status != 200) {
        throw std::runtime_error("GET " + url + ": HTTP " + std::to_string(res.status) + ": " + res.body.substr(0, 500));
    }
    const json j = json::parse(res.body);
    JobStatus st;
    st.job_id = j.at("job_id").get<int>();
    st.client_id = j.at("client_id").get<std::string>();
    st.sweep = j.at("sweep").get<int>();
    st.status = j.at("status").get<std::string>();
    if (j.contains("error") && j["error"].is_string()) st.error = j["error"].get<std::string>();
    if (st.status == "done") {
        st.n_length = j.value("n_length", 0);
        for (const auto& r : j.at("results")) {
            st.results.push_back({
                r.at("id").get<std::string>(),
                r.at("response").get<std::string>(),
                r.at("finish_reason").is_string() ? r.at("finish_reason").get<std::string>() : "",
            });
        }
    }
    return st;
}

std::string JobClient::info() {
    HttpResponse res = withRetry("GET /info", [&] { return transport_->get(base_url_ + "/info", options_.get_timeout_sec); });
    if (res.status != 200) throw std::runtime_error("GET /info: HTTP " + std::to_string(res.status));
    return res.body;
}
