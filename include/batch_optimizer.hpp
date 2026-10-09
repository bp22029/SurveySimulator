#ifndef BATCH_OPTIMIZER_HPP
#define BATCH_OPTIMIZER_HPP

// 一括方式の SA による個性特性（BFI-2）の最適化（設計書 §4）。
//
// 1周（全員を1回ずつ、周ごとにシャッフルした順番）の変更案を先に作り、全員分の推論を
// 1ジョブとして job_server に投げる。結果が返ったら、採否の判定だけを順番どおりに行う。
// 各人の回答はその人の属性と個性だけで決まるので、1人ずつ逐次に推論する方法と同じ手順になる。
//
// 昨年の逐次版（experiment_runner.cpp）・並列版（experiment_runner_parallel.cpp）は残してあり、
// このファイルはそれらを使わない。

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>
#include "job_client.hpp"
#include "person.hpp"
#include "question.hpp"
#include "tae_tracker.hpp"

using Answers = TaeTracker::Answers;

struct BatchOptimizerConfig {
    // job_server の URL（例: http://192.168.130.XXX:8000）。設定ファイルには書かず、環境変数 JOB_SERVER_URL か
    // カレントディレクトリの .env で指定する（resolveServerUrl）。再現性の照合には使わない
    std::string server_url;
    std::string server_url_source;   // どこから取ったか（表示用）
    std::string client_id;           // 例: bp22029
    // 推論条件（研究ごとに決める。最適化の途中では変えない。省略不可）
    bool enable_thinking = true;     // 思考の有無
    std::string reasoning_effort;    // xhigh | medium | low。思考なしのときは書かない（空）
    int max_tokens = 0;              // 出力（思考を含む）のトークン数の上限
    int sweeps = 0;                  // 周回数 S
    double initial_temperature = 0.0;
    double final_temperature = 0.0;
    double mutation_step_size = 0.05;
    uint64_t seed = 42;
    std::string population_csv;      // 合成人口（属性のみ）
    std::string questions_csv;
    std::string system_prompt_path;
    std::string user_prompt_path;
    std::string real_ratios_csv;
    std::string run_dir;             // チェックポイントとログの出力先
    int poll_interval_sec = 30;
    int max_job_attempts = 3;        // failed になったジョブを投げ直す回数の上限
};

// JSON ファイルから読む。server_url は resolveServerUrl で決める
BatchOptimizerConfig loadBatchOptimizerConfig(const std::string& path);

// job_server の URL を決める。優先順：環境変数 JOB_SERVER_URL → env_file（KEY=VALUE 形式）の JOB_SERVER_URL →
// 設定ファイルの server_url（古い設定ファイルのため）。どれもなければ例外。戻り値は (URL, 取った場所)
std::pair<std::string, std::string> resolveServerUrl(const std::string& config_value,
                                                     const std::string& env_file = ".env");

// KEY=VALUE 形式のファイルから1つの値を読む（# のコメント、空行、先頭の export、値の引用符を扱う）。なければ空
std::string readEnvFileValue(const std::string& path, const std::string& key);

// 入力ファイル（人口・質問・テンプレート・正解比率）に CR（\r）が含まれていたら例外。
// 既存の読み込み関数は \r を取り除かないので、改行が CRLF だとプロンプトが変わってしまう
void requireLfInputFiles(const BatchOptimizerConfig& config);

// 設定ファイルが指す入力（CRLF の確認込み）。population の個性はまだ決まっていない
struct OptimizerInputs {
    std::vector<Person> population;
    std::vector<Question> questions;
    std::string system_template;
    std::string user_template;
    std::map<std::string, std::vector<double>> real_ratios;
};
OptimizerInputs loadOptimizerInputs(const BatchOptimizerConfig& config);

std::vector<std::string> questionIds(const std::vector<Question>& questions);

// 一時ファイルに書いてから置き換える（途中で止まっても壊れたファイルを残さない）
void writeFileAtomic(const std::filesystem::path& path, const std::string& content);

// --- 1周の中身（テストのために公開） ----------------------------------

// BFI-2 の15下位特性を、決まった順番で返す
std::vector<float*> facetPointers(Person& person);
std::vector<float> facetValues(const Person& person);
void setFacetValues(Person& person, const std::vector<float>& values);

// 全員の15下位特性を [0,1) の一様乱数で決める（人の順番 × 下位特性の順番で乱数を引く）
void assignInitialPersonality(std::vector<Person>& population, std::mt19937_64& rng);

// 15下位特性すべてに N(0, step_size) を足して [0,1] に収める
Person perturbPerson(const Person& origin, double step_size, std::mt19937_64& rng);

struct SweepPlan {
    std::vector<int> order;          // 採否を判定する順番（population の添字）
    std::vector<Person> proposals;   // population と同じ並びの変更案
};

// 順番をシャッフルし、その順番で全員の変更案を作る
SweepPlan planSweep(const std::vector<Person>& population, double step_size, std::mt19937_64& rng);

// 全員分のプロンプト（population の並び × 質問の並び）。id は "personID_questionID"
std::vector<JobPrompt> buildPrompts(const std::vector<Person>& persons, const std::vector<Question>& questions,
                                    const std::string& system_template, const std::string& user_template);

// 思考の後の最後の <answer>N</answer> を取り出す。見つからなければ -1
int extractFinalAnswer(const std::string& response);

struct ParsedResults {
    std::map<int, Answers> answers;  // person_id -> 解析できた回答（選択肢の範囲外は除く）
    int n_unparsed = 0;
    int n_length = 0;                // finish_reason == "length" の件数
};

// ジョブの結果を解析する。投げた id と返ってきた id が一致しなければ例外
ParsedResults parseResults(const std::vector<JobOutput>& outputs, const std::vector<JobPrompt>& prompts,
                           const std::vector<Question>& questions);

double coolingRate(double initial_temperature, double final_temperature, int sweeps, int population_size);

struct Decision {
    int position;        // 周の中で何番目か
    int person_id;
    double delta;        // TAE の変化
    bool accepted;
    bool skipped;        // 回答が1件も解析できず、判定しなかった
    double temperature;  // 判定に使った温度
    double tae;          // 判定後の TAE
};

// 順番どおりに採否を判定する。answers と population と tracker と temperature を更新する
// 回答が1件も解析できなかった人は、乱数を引かずに不採用とする（冷却は行う）
std::vector<Decision> applySweep(const SweepPlan& plan, const ParsedResults& parsed,
                                 std::vector<Person>& population, std::map<int, Answers>& answers,
                                 TaeTracker& tracker, double& temperature, double alpha,
                                 std::mt19937_64& rng);

// 個性と回答の CSV。IndividualResponseManager::exportMergedPopulationCSV_BFI2 と同じ列構成・同じ書式。
// あちらは recordResponse が1件ごとに標準出力へ書く（1周で約2万行）ので、こちらで直接書く
void writePopulationCsv(const std::string& path, const std::vector<Person>& population,
                        const std::map<int, Answers>& answers, const std::vector<std::string>& question_ids);

// --- 全体 ---------------------------------------------------------------

// ジョブを投げて done になるまで待つ。failed なら max_job_attempts 回まで投げ直す。
// 返ってきた結果の推論条件が設定と違えば例外
JobStatus runJob(const BatchOptimizerConfig& config, JobClient& client, const std::string& client_id, int sweep,
                 const std::vector<JobPrompt>& prompts, const std::function<void(int)>& sleep_sec);

// チェックポイントがあれば続きから、なければ初期化（sweep 0 = 初期個性での推論）から始める
void runBatchOptimization(const BatchOptimizerConfig& config, JobClient& client,
                          const std::function<void(int)>& sleep_sec = nullptr);

#endif // BATCH_OPTIMIZER_HPP
