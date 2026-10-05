// 一括方式（job_server を使う）の最適化のテスト
#include "gtest/gtest.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include "nlohmann/json.hpp"
#include "batch_optimizer.hpp"
#include "individual_response_manager.hpp"
#include "job_client.hpp"
#include "optimization_manager.hpp"
#include "portable_random.hpp"
#include "tae_tracker.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// ==========================================
// portable_random
// ==========================================

TEST(PortableRandomTest, EngineMatchesStandard) {
    // 規格が定める mt19937_64 の 10000 番目の出力
    std::mt19937_64 rng;
    rng.discard(9999);
    EXPECT_EQ(rng(), 9981545732273789042ULL);
}

TEST(PortableRandomTest, UniformIntStaysInRange) {
    std::mt19937_64 rng(1);
    for (uint64_t n : {1ULL, 2ULL, 3ULL, 403ULL, 1000003ULL}) {
        for (int i = 0; i < 1000; ++i) EXPECT_LT(portable_random::uniformInt(rng, n), n);
    }
    EXPECT_THROW(portable_random::uniformInt(rng, 0), std::invalid_argument);
}

TEST(PortableRandomTest, UniformRealStaysInHalfOpenUnitInterval) {
    std::mt19937_64 rng(2);
    for (int i = 0; i < 10000; ++i) {
        double u = portable_random::uniformReal(rng);
        EXPECT_GE(u, 0.0);
        EXPECT_LT(u, 1.0);
    }
}

TEST(PortableRandomTest, ShuffleIsAPermutation) {
    std::mt19937_64 rng(3);
    std::vector<int> v(403);
    for (int i = 0; i < 403; ++i) v[i] = i;
    portable_random::shuffle(v, rng);
    std::set<int> s(v.begin(), v.end());
    EXPECT_EQ(s.size(), 403u);
    EXPECT_EQ(*s.begin(), 0);
    EXPECT_EQ(*s.rbegin(), 402);

    std::vector<int> empty;
    portable_random::shuffle(empty, rng); // 落ちない
}

TEST(PortableRandomTest, NormalConsumesExactlyTwoDraws) {
    std::mt19937_64 a(4), b(4);
    portable_random::normal(a, 0.0, 1.0);
    b.discard(2);
    EXPECT_EQ(a(), b());
}

TEST(PortableRandomTest, StateRoundTrip) {
    std::mt19937_64 a(5);
    a.discard(123);
    std::mt19937_64 b;
    portable_random::loadState(b, portable_random::saveState(a));
    for (int i = 0; i < 100; ++i) EXPECT_EQ(a(), b());
}

// 2人の環境でこの値が一致することを確認するための固定値（Docker の Ubuntu 24.04 / g++ 13.3 /
// glibc 2.39 で記録）。整数演算だけで決まるものは完全一致、libm を使う normal は最後の数ビットまで
TEST(PortableRandomTest, FixedValuesForSeed42) {
    std::mt19937_64 rng(42);
    EXPECT_EQ(portable_random::uniformInt(rng, 403), 14u);
    EXPECT_EQ(portable_random::uniformReal(rng), 5755883094484128 * 0x1.0p-53);
    std::vector<int> v = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    portable_random::shuffle(v, rng);
    EXPECT_EQ(v, (std::vector<int>{9, 3, 1, 2, 7, 4, 8, 5, 6, 0}));
    EXPECT_DOUBLE_EQ(portable_random::normal(rng, 0.0, 1.0), -0.48180611448222493);
}

// ==========================================
// TaeTracker
// ==========================================

namespace {

const std::vector<std::string> kQids = {"dq2_1", "dq3_1", "dq9_1", "dq22_1", "dq_nodata"};

std::map<std::string, std::vector<double>> realRatios() {
    return {
        {"dq2_1", {0.3, 0.3, 0.4}},
        {"dq3_1", {0.6, 0.4}},
        {"dq9_1", {0.5, 0.25, 0.25}},
        {"dq22_1", {0.1, 0.7, 0.2}},
    };
}

std::map<int, Answers> randomAnswers(int n, std::mt19937_64& rng) {
    std::map<int, Answers> all;
    for (int p = 0; p < n; ++p) {
        for (const auto& q : kQids) {
            if (portable_random::uniformInt(rng, 10) == 0) continue; // 未回答
            all[100 + p][q] = 1 + static_cast<int>(portable_random::uniformInt(rng, 5));
        }
    }
    return all;
}

} // namespace

TEST(TaeTrackerTest, MatchesOptimizationManager) {
    std::mt19937_64 rng(7);
    const auto answers = randomAnswers(40, rng);

    // OptimizationManager は CSV からしか読めないので一時ファイルに書く
    fs::path csv = fs::temp_directory_path() / "tae_tracker_real_ratios.csv";
    {
        std::ofstream out(csv);
        out << "question_id,b1,b2,b3\n";
        for (const auto& [q, r] : realRatios()) {
            out << q;
            for (double x : r) out << "," << x;
            out << "\n";
        }
    }
    OptimizationManager opt;
    ASSERT_TRUE(opt.loadRealData(csv.string()));
    IndividualResponseManager manager;
    for (const auto& [pid, a] : answers)
        for (const auto& [q, c] : a) manager.recordResponse(pid, q, c);
    opt.initializeCounts(manager, 40, kQids);

    TaeTracker tracker;
    ASSERT_TRUE(tracker.loadRealData(csv.string()));
    tracker.initialize(answers, 40, kQids);
    EXPECT_NEAR(tracker.total(), opt.getCurrentTotalMAE(), 1e-12);

    // 置き換えの評価も一致する
    Answers old_a = answers.at(100);
    Answers new_a = {{"dq2_1", 5}, {"dq3_1", 1}, {"dq9_1", 4}};
    EXPECT_NEAR(tracker.evaluate(old_a, new_a), opt.tryUpdate(old_a, new_a), 1e-12);
    fs::remove(csv);
}

TEST(TaeTrackerTest, CommittedTotalEqualsRecomputedTotalExactly) {
    // 再開したときに TAE が通しで回した場合と一致するための性質
    std::mt19937_64 rng(8);
    auto answers = randomAnswers(30, rng);
    TaeTracker tracker;
    tracker.setRealData(realRatios());
    tracker.initialize(answers, 30, kQids);

    for (int step = 0; step < 200; ++step) {
        int pid = 100 + static_cast<int>(portable_random::uniformInt(rng, 30));
        Answers new_a;
        for (const auto& q : kQids) new_a[q] = 1 + static_cast<int>(portable_random::uniformInt(rng, 5));
        double predicted = tracker.evaluate(answers[pid], new_a);
        tracker.commit(answers[pid], new_a);
        for (const auto& [q, c] : new_a) answers[pid][q] = c;
        EXPECT_EQ(tracker.total(), predicted);
    }
    TaeTracker fresh;
    fresh.setRealData(realRatios());
    fresh.initialize(answers, 30, kQids);
    EXPECT_EQ(tracker.total(), fresh.total());
}

TEST(TaeTrackerTest, BinMappingMatchesOptimizationManagerExamples) {
    EXPECT_EQ(TaeTracker::mapChoiceToBin("dq2_3", 2), 0);
    EXPECT_EQ(TaeTracker::mapChoiceToBin("dq2_3", 3), 1);
    EXPECT_EQ(TaeTracker::mapChoiceToBin("dq2_3", 5), 2);
    EXPECT_EQ(TaeTracker::mapChoiceToBin("dq9_2", 3), 0);
    EXPECT_EQ(TaeTracker::mapChoiceToBin("dq22_6", 4), 1);
    EXPECT_EQ(TaeTracker::mapChoiceToBin("dq10_1", 3), 1);
    EXPECT_EQ(TaeTracker::mapChoiceToBin("dq2_1", 0), -1);
    EXPECT_EQ(TaeTracker::mapChoiceToBin("unknown", 1), -1);
}

// ==========================================
// 回答の解析
// ==========================================

TEST(BatchOptimizerTest, ExtractFinalAnswerIgnoresDraftsInsideThinking) {
    EXPECT_EQ(extractFinalAnswer("<think>まず <answer>2</answer> と考えた</think>\n<answer>4</answer>"), 4);
    EXPECT_EQ(extractFinalAnswer("<think>a</think><think>b</think><answer> 3 </answer>"), 3);
    EXPECT_EQ(extractFinalAnswer("<answer>1</answer>"), 1);       // 思考なし
    EXPECT_EQ(extractFinalAnswer("<think><answer>2</answer></think>答えられません"), -1);
    EXPECT_EQ(extractFinalAnswer(""), -1);
}

namespace {

std::vector<Question> testQuestions() {
    return {
        {"dq2_1", "Q1", {"a", "b", "c", "d", "e"}},
        {"dq3_1", "Q2", {"a", "b", "c", "d"}},
    };
}

} // namespace

TEST(BatchOptimizerTest, ParseResultsValidatesIdsAndChoices) {
    std::vector<JobPrompt> prompts = {{"7_dq2_1", "", ""}, {"7_dq3_1", "", ""}, {"8_dq2_1", "", ""}, {"8_dq3_1", "", ""}};
    std::vector<JobOutput> outputs = {
        {"7_dq2_1", "</think><answer>5</answer>", "stop"},
        {"7_dq3_1", "</think><answer>5</answer>", "stop"},   // 選択肢は4つまで
        {"8_dq2_1", "<think>...<answer>1</answer>", "length"},
        {"8_dq3_1", "</think>no answer", "stop"},
    };
    ParsedResults parsed = parseResults(outputs, prompts, testQuestions());
    EXPECT_EQ(parsed.answers.at(7), (Answers{{"dq2_1", 5}}));
    EXPECT_TRUE(parsed.answers.at(8).empty());
    EXPECT_EQ(parsed.n_unparsed, 3);
    EXPECT_EQ(parsed.n_length, 1);

    outputs.pop_back();
    EXPECT_THROW(parseResults(outputs, prompts, testQuestions()), std::runtime_error);
    outputs.push_back({"9_dq3_1", "", "stop"});
    EXPECT_THROW(parseResults(outputs, prompts, testQuestions()), std::runtime_error);
}

// ==========================================
// 1周の処理
// ==========================================

namespace {

std::vector<Person> testPopulation(int n) {
    std::vector<Person> pop(n);
    for (int i = 0; i < n; ++i) pop[i].person_id = 100 + i;
    std::mt19937_64 rng(11);
    assignInitialPersonality(pop, rng);
    return pop;
}

} // namespace

TEST(BatchOptimizerTest, PerturbKeepsFacetsInUnitIntervalAndUpdatesScores) {
    auto pop = testPopulation(1);
    std::mt19937_64 rng(12);
    Person p = perturbPerson(pop[0], 0.8, rng);
    for (float v : facetValues(p)) {
        EXPECT_GE(v, 0.0f);
        EXPECT_LE(v, 1.0f);
    }
    const auto& n = p.personality.neuroticism;
    EXPECT_FLOAT_EQ(n.score, (n.anxiety + n.depression + n.emotional_volatility) / 3.0f);
    EXPECT_NE(facetValues(p), facetValues(pop[0]));
}

TEST(BatchOptimizerTest, PlanSweepVisitsEveryoneOnceAndIsDeterministic) {
    auto pop = testPopulation(20);
    std::mt19937_64 a(13), b(13);
    SweepPlan pa = planSweep(pop, 0.05, a);
    SweepPlan pb = planSweep(pop, 0.05, b);
    EXPECT_EQ(pa.order, pb.order);
    EXPECT_EQ(std::set<int>(pa.order.begin(), pa.order.end()).size(), 20u);
    for (size_t i = 0; i < pop.size(); ++i) {
        EXPECT_EQ(pa.proposals[i].person_id, pop[i].person_id);
        EXPECT_EQ(facetValues(pa.proposals[i]), facetValues(pb.proposals[i]));
    }
    // 1回の計画で、シャッフル（19回）と変更案（20人 × 15下位特性 × 2個）の乱数を使う
    EXPECT_EQ(a(), b());
}

TEST(BatchOptimizerTest, ApplySweepDrawsRandomOnlyWhenNeededAndCoolsPerPerson) {
    auto pop = testPopulation(3);
    TaeTracker tracker;
    tracker.setRealData({{"dq2_1", {1.0, 0.0, 0.0}}});
    std::map<int, Answers> answers = {{100, {{"dq2_1", 5}}}, {101, {{"dq2_1", 5}}}, {102, {{"dq2_1", 1}}}};
    tracker.initialize(answers, 3, {"dq2_1"});

    SweepPlan plan;
    plan.order = {2, 0, 1};
    plan.proposals = pop;
    ParsedResults parsed;
    parsed.answers[100] = {{"dq2_1", 1}}; // 改善 → 乱数を引かずに採用
    parsed.answers[101] = {};             // 解析できず → 乱数を引かずに不採用
    parsed.answers[102] = {{"dq2_1", 5}}; // 悪化 → 乱数で判定

    std::mt19937_64 rng(14), expected(14);
    double t = 1.0;
    auto decisions = applySweep(plan, parsed, pop, answers, tracker, t, 0.5, rng);

    ASSERT_EQ(decisions.size(), 3u);
    EXPECT_EQ(decisions[0].person_id, 102);
    EXPECT_GT(decisions[0].delta, 0.0);
    EXPECT_EQ(decisions[0].accepted, portable_random::uniformReal(expected) < std::exp(-decisions[0].delta / 1.0));
    EXPECT_EQ(decisions[1].person_id, 100);
    EXPECT_DOUBLE_EQ(decisions[1].temperature, 0.5);
    EXPECT_TRUE(decisions[2].skipped);
    EXPECT_FALSE(decisions[2].accepted);
    EXPECT_DOUBLE_EQ(t, 0.125);
    EXPECT_EQ(rng(), expected()); // 乱数は1個だけ使った
    EXPECT_EQ(answers.at(100).at("dq2_1"), decisions[1].accepted ? 1 : 5);
}

TEST(BatchOptimizerTest, PopulationCsvIsIdenticalToOldExporter) {
    auto pop = testPopulation(4);
    pop[1].gender = "女性";
    pop[2].total_income = 250;
    std::map<int, Answers> answers = {{100, {{"dq2_1", 3}, {"dq3_1", 1}}}, {102, {{"dq3_1", 4}}}};
    const std::vector<std::string> qids = {"dq2_1", "dq3_1"};

    IndividualResponseManager manager;
    for (const auto& [pid, a] : answers)
        for (const auto& [q, c] : a) manager.recordResponse(pid, q, c);
    fs::path old_csv = fs::temp_directory_path() / "population_old_exporter.csv";
    fs::path new_csv = fs::temp_directory_path() / "population_new_exporter.csv";
    manager.exportMergedPopulationCSV_BFI2(old_csv.string(), pop, qids);
    writePopulationCsv(new_csv.string(), pop, answers, qids);

    auto read = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    };
    EXPECT_EQ(read(new_csv), read(old_csv));
    fs::remove(old_csv);
    fs::remove(new_csv);
}

TEST(BatchOptimizerTest, CoolingRateReachesFinalTemperature) {
    double alpha = coolingRate(0.02, 0.002, 50, 403);
    double t = 0.02;
    for (int i = 0; i < 50 * 403; ++i) t *= alpha;
    EXPECT_NEAR(t, 0.002, 1e-12);
}

// ==========================================
// 全体（偽の job_server を使う）
// ==========================================

namespace {

// 回答は「システムプロンプト（個性を含む）＋ユーザープロンプト」から決まる、決定的な偽の推論
class FakeJobServer : public HttpTransport {
public:
    std::string model = "fake-model";
    int fail_post_for_sweep = -1;  // この周の POST で「クライアントが落ちた」ことにする
    int fail_job_once_for_sweep = -1;
    int posts = 0;

    HttpResponse postJson(const std::string& url, const std::string& body, long) override {
        json j = json::parse(body);
        int sweep = j["sweep"];
        if (sweep == fail_post_for_sweep) {
            fail_post_for_sweep = -1;
            throw std::runtime_error("simulated client crash");
        }
        ++posts;
        std::string key = j["client_id"].get<std::string>() + "#" + std::to_string(sweep);
        auto it = by_key_.find(key);
        if (it != by_key_.end()) {
            Job& job = jobs_[it->second - 1];
            if (job.requests != j["requests"]) return {409, "{\"detail\":\"mismatch\"}", ""};
            if (job.status == "failed") job.status = "done";
            return {200, json{{"job_id", job.id}, {"created", false}}.dump(), ""};
        }
        Job job{static_cast<int>(jobs_.size()) + 1, j["client_id"], sweep, j["requests"], "done"};
        if (sweep == fail_job_once_for_sweep) {
            fail_job_once_for_sweep = -1;
            job.status = "failed";
        }
        jobs_.push_back(job);
        by_key_[key] = job.id;
        return {200, json{{"job_id", job.id}, {"created", true}}.dump(), ""};
    }

    HttpResponse get(const std::string& url, long) override {
        if (url.find("/info") != std::string::npos) {
            return {200, json{{"model", model}, {"revision", "abc"}, {"git", {{"commit", "x"}}}}.dump(), ""};
        }
        auto q = url.find("/jobs?");
        if (q != std::string::npos) {
            json list = json::array();
            for (const auto& job : jobs_) {
                std::string query = "client_id=" + job.client_id + "&sweep=" + std::to_string(job.sweep);
                if (url.substr(q + 6) == query) list.push_back({{"job_id", job.id}, {"status", job.status}});
            }
            return {200, json{{"jobs", list}}.dump(), ""};
        }
        int id = std::stoi(url.substr(url.rfind('/') + 1));
        const Job& job = jobs_.at(id - 1);
        json body = {{"job_id", job.id}, {"client_id", job.client_id}, {"sweep", job.sweep}, {"status", job.status}};
        if (job.status == "failed") body["error"] = "CUDA error (simulated)";
        if (job.status == "done") {
            json results = json::array();
            for (const auto& r : job.requests) {
                std::string text = r["system_prompt"].get<std::string>() + r["user_prompt"].get<std::string>();
                int choice = 1 + static_cast<int>(std::hash<std::string>{}(text) % 4);
                results.push_back({{"id", r["id"]},
                                   {"response", "<think>考え中</think>\n<answer>" + std::to_string(choice) + "</answer>"},
                                   {"finish_reason", "stop"}});
            }
            body["results"] = results;
            body["n_length"] = 0;
        }
        return {200, body.dump(), ""};
    }

private:
    struct Job {
        int id;
        std::string client_id;
        int sweep;
        json requests;
        std::string status;
    };
    std::vector<Job> jobs_;
    std::map<std::string, int> by_key_;
};

class BatchRunTest : public ::testing::Test {
protected:
    fs::path dir;

    void SetUp() override {
        dir = fs::temp_directory_path() /
              ("batch_run_test_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir);
        fs::create_directories(dir);

        std::ofstream pop(dir / "population.csv");
        pop << "\"prefecture_code\",\"prefecture_name\",\"city_code\",\"city_name\",\"town_code\",\"town_name\","
               "\"latitude\",\"longitude\",\"household_id\",\"family_type_id\",\"family_type\",\"num_member\","
               "\"abnormal_household\",\"person_id\",\"age\",\"gender_id\",\"gender\",\"role_household_type_id\","
               "\"role_household_type\",\"industry_type_id\",\"industry_type\",\"employment_type_id\","
               "\"employment_type\",\"company_size_id\",\"company_size\",\"total_income\"\n";
        for (int i = 0; i < 12; ++i) {
            pop << "\"47\",\"沖縄県\",\"47356\",\"島尻郡渡名喜村\",\"1\",\"\",\"26.3\",\"127.1\",\"" << i
                << "\",\"0\",\"単独世帯\",\"1\",\"0\",\"" << (500 + i) << "\",\"" << (20 + 3 * i)
                << "\",\"0\",\"" << (i % 2 ? "女性" : "男性") << "\",\"0\",\"単独世帯\",\"1\",\"建設業\",\"1\","
                << "\"正規の職員・従業員\",\"1\",\"1～4人\",\"" << (100 + i) << "\"\n";
        }
        std::ofstream q(dir / "questions.csv");
        q << "question_id,full_question_text,option_1,option_2,option_3,option_4,option_5\n"
          << "dq2_1,質問1,1,2,3,4,5\n"
          << "dq3_1,質問2,1,2,3,4\n"
          << "dq9_1,質問3,1,2,3,4,5\n";
        std::ofstream r(dir / "real_ratios.csv");
        r << "question_id,b1,b2,b3\n"
          << "dq2_1,0.5,0.2,0.3\n"
          << "dq3_1,0.7,0.3\n"
          << "dq9_1,0.6,0.3,0.1\n";
        std::ofstream(dir / "system.txt") << "年齢{年齢} 不安{不安} 社交性{社交性} 信用{信用}\n";
        std::ofstream(dir / "user.txt") << "{質問}\n{回答選択肢}";
    }

    void TearDown() override { fs::remove_all(dir); }

    BatchOptimizerConfig config(const std::string& run) const {
        BatchOptimizerConfig c;
        c.server_url = "http://fake";
        c.client_id = "tester";
        c.sweeps = 4;
        c.initial_temperature = 0.05;
        c.final_temperature = 0.005;
        c.mutation_step_size = 0.2;
        c.seed = 42;
        c.population_csv = (dir / "population.csv").string();
        c.questions_csv = (dir / "questions.csv").string();
        c.system_prompt_path = (dir / "system.txt").string();
        c.user_prompt_path = (dir / "user.txt").string();
        c.real_ratios_csv = (dir / "real_ratios.csv").string();
        c.run_dir = (dir / run).string();
        c.poll_interval_sec = 0;
        return c;
    }

    static json checkpoint(const BatchOptimizerConfig& c) {
        std::ifstream in(fs::path(c.run_dir) / "checkpoint.json");
        return json::parse(in);
    }

    static JobClient client(const std::shared_ptr<FakeJobServer>& server) {
        JobClientOptions opt;
        opt.retry_wait_sec = 0;
        return JobClient("http://fake", server, opt, [](int) {});
    }
};

// 再現性の比較に使う部分（保存時刻などを除く）
json comparable(json cp) {
    cp.erase("saved_at");
    for (auto& h : cp["history"]) h.erase("finished_at");
    return cp;
}

const auto noSleep = [](int) {};

} // namespace

TEST_F(BatchRunTest, RunsAllSweepsAndWritesOutputs) {
    auto server = std::make_shared<FakeJobServer>();
    auto c = config("run");
    auto cl = client(server);
    runBatchOptimization(c, cl, noSleep);

    json cp = checkpoint(c);
    EXPECT_EQ(cp["completed_sweep"], 4);
    EXPECT_EQ(cp["history"].size(), 5u);
    EXPECT_EQ(cp["personality"].size(), 12u);
    EXPECT_NEAR(cp["temperature"].get<double>(), 0.005, 1e-12);
    EXPECT_EQ(server->posts, 5);
    EXPECT_TRUE(fs::exists(fs::path(c.run_dir) / "sweeps.csv"));
    EXPECT_TRUE(fs::exists(fs::path(c.run_dir) / "sweeps" / "sweep_004_decisions.csv"));
    EXPECT_TRUE(fs::exists(fs::path(c.run_dir) / "sweeps" / "sweep_004_population.csv"));

    // チェックポイントの TAE は、保存された回答から計算し直した値と一致する
    std::map<int, Answers> answers;
    for (const auto& [pid, a] : cp["answers"].items()) answers[std::stoi(pid)] = a.get<Answers>();
    TaeTracker t;
    ASSERT_TRUE(t.loadRealData(c.real_ratios_csv));
    t.initialize(answers, 12, {"dq2_1", "dq3_1", "dq9_1"});
    EXPECT_EQ(cp["tae"].get<double>(), t.total());
}

TEST_F(BatchRunTest, ResumingAfterCrashGivesSameResultAsUninterruptedRun) {
    auto c_full = config("full");
    {
        auto server = std::make_shared<FakeJobServer>();
        auto cl = client(server);
        runBatchOptimization(c_full, cl, noSleep);
    }

    auto c_resumed = config("resumed");
    auto server = std::make_shared<FakeJobServer>();
    server->fail_post_for_sweep = 3;
    {
        auto cl = client(server);
        EXPECT_THROW(runBatchOptimization(c_resumed, cl, noSleep), std::runtime_error);
        EXPECT_EQ(checkpoint(c_resumed)["completed_sweep"], 2);
    }
    {
        auto cl = client(server);
        runBatchOptimization(c_resumed, cl, noSleep);
    }
    json a = comparable(checkpoint(c_full));
    json b = comparable(checkpoint(c_resumed));
    a.erase("settings");
    b.erase("settings");
    EXPECT_EQ(a["rng_state"], b["rng_state"]);
    EXPECT_EQ(a["answers"], b["answers"]);
    EXPECT_EQ(a["personality"], b["personality"]);
    EXPECT_EQ(a["tae"], b["tae"]);
    EXPECT_EQ(a["temperature"], b["temperature"]);
    EXPECT_EQ(a["history"], b["history"]);
}

TEST_F(BatchRunTest, ResumingReusesJobAlreadyOnServer) {
    auto c = config("reuse");
    auto server = std::make_shared<FakeJobServer>();
    auto cl = client(server);
    c.sweeps = 2;
    runBatchOptimization(c, cl, noSleep);
    // チェックポイントを1周前に戻す（周の推論後、保存前に落ちた状況）
    json cp1;
    {
        auto c1 = config("reuse_from_scratch");
        c1.sweeps = 2;
        auto server1 = std::make_shared<FakeJobServer>();
        server1->fail_post_for_sweep = 2;
        auto cl1 = client(server1);
        EXPECT_THROW(runBatchOptimization(c1, cl1, noSleep), std::runtime_error);
        cp1 = checkpoint(c1);
    }
    cp1["settings"] = checkpoint(c)["settings"];
    std::ofstream(fs::path(c.run_dir) / "checkpoint.json") << cp1.dump();

    int posts_before = server->posts;
    runBatchOptimization(c, cl, noSleep);
    EXPECT_EQ(server->posts, posts_before + 1);           // 内容の確認のための POST だけ
    EXPECT_EQ(checkpoint(c)["job_ids"]["2"], 3);         // 新しいジョブは作られていない
}

TEST_F(BatchRunTest, RefusesCrlfInputFiles) {
    auto c = config("crlf");
    EXPECT_NO_THROW(requireLfInputFiles(c));
    std::ofstream(dir / "user.txt", std::ios::binary) << "{質問}\r\n{回答選択肢}\r\n";
    EXPECT_THROW(requireLfInputFiles(c), std::runtime_error);
    auto server = std::make_shared<FakeJobServer>();
    auto cl = client(server);
    EXPECT_THROW(runBatchOptimization(c, cl, noSleep), std::runtime_error);
    EXPECT_EQ(server->posts, 0);
}

TEST_F(BatchRunTest, FailedJobIsResubmitted) {
    auto c = config("failed");
    auto server = std::make_shared<FakeJobServer>();
    server->fail_job_once_for_sweep = 1;
    auto cl = client(server);
    runBatchOptimization(c, cl, noSleep);
    EXPECT_EQ(checkpoint(c)["completed_sweep"], 4);
}

TEST_F(BatchRunTest, RefusesToResumeWithChangedSettings) {
    auto c = config("changed");
    c.sweeps = 1;
    auto server = std::make_shared<FakeJobServer>();
    auto cl = client(server);
    runBatchOptimization(c, cl, noSleep);

    auto changed = c;
    changed.mutation_step_size = 0.1;
    EXPECT_THROW(runBatchOptimization(changed, cl, noSleep), std::runtime_error);
}

TEST_F(BatchRunTest, RefusesToResumeWhenServerModelChanged) {
    auto c = config("server_changed");
    c.sweeps = 1;
    auto server = std::make_shared<FakeJobServer>();
    auto cl = client(server);
    runBatchOptimization(c, cl, noSleep);

    server->model = "another-model";
    EXPECT_THROW(runBatchOptimization(c, cl, noSleep), std::runtime_error);
}
