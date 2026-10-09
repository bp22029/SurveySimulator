#include "../include/calibrate_temperature.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include "nlohmann/json.hpp"
#include "../include/tae_tracker.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// μAG比の目標（山田らの方法。昨年度と同じ）
constexpr double kInitialAcceptance = 0.5;
constexpr double kFinalAcceptance = 0.002;

DeltaStats summarizeDeltas(const std::vector<double>& deltas, int n_skipped) {
    DeltaStats s;
    s.n_proposals = static_cast<int>(deltas.size());
    s.n_skipped = n_skipped;
    double sum = 0.0;
    for (double d : deltas) {
        if (d > kDeltaTolerance) {
            s.n_worse++;
            sum += d;
        } else if (d < -kDeltaTolerance) {
            s.n_better++;
        } else {
            s.n_zero++;
        }
    }
    s.mean_worse = s.n_worse > 0 ? sum / s.n_worse : 0.0;
    return s;
}

std::vector<double> worseDeltas(const std::vector<double>& deltas) {
    std::vector<double> worse;
    for (double d : deltas) {
        if (d > kDeltaTolerance) worse.push_back(d);
    }
    return worse;
}

double temperatureFromMeanDelta(double mean_worse, double acceptance) {
    return mean_worse / std::log(1.0 / acceptance);
}

double expectedAcceptance(const std::vector<double>& worse, double temperature) {
    if (worse.empty()) throw std::invalid_argument("expectedAcceptance: no worse deltas");
    double sum = 0.0;
    for (double d : worse) sum += std::exp(-d / temperature);
    return sum / static_cast<double>(worse.size());
}

double temperatureForAcceptance(const std::vector<double>& worse, double acceptance) {
    if (worse.empty()) throw std::invalid_argument("temperatureForAcceptance: no worse deltas");
    if (!(acceptance > 0.0 && acceptance < 1.0)) throw std::invalid_argument("acceptance must be in (0, 1)");
    // 受け入れる割合は温度について単調に増えるので、対数スケールで二分法
    double lo = 1e-12, hi = 1.0;
    while (expectedAcceptance(worse, hi) < acceptance) hi *= 2.0;
    for (int i = 0; i < 200; ++i) {
        double mid = std::sqrt(lo * hi);
        if (expectedAcceptance(worse, mid) < acceptance) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return std::sqrt(lo * hi);
}

namespace {

std::string timeString() {
    std::time_t t = std::time(nullptr);
    std::ostringstream os;
    os << std::put_time(std::localtime(&t), "%Y-%m-%dT%H:%M:%S");
    return os.str();
}

json statsJson(const DeltaStats& s) {
    return {{"n_proposals", s.n_proposals}, {"n_skipped", s.n_skipped}, {"n_worse", s.n_worse},
            {"n_better", s.n_better}, {"n_zero", s.n_zero}, {"mean_worse", s.mean_worse}};
}

// 2つの方法で温度を出し、それぞれの温度で実際に受け入れる割合（期待値）も添える
json temperaturesJson(const std::vector<double>& worse, double mean_worse, int sweeps, int population_size) {
    const double t0_mean = temperatureFromMeanDelta(mean_worse, kInitialAcceptance);
    const double tf_mean = temperatureFromMeanDelta(mean_worse, kFinalAcceptance);
    const double t0_dist = temperatureForAcceptance(worse, kInitialAcceptance);
    const double tf_dist = temperatureForAcceptance(worse, kFinalAcceptance);
    auto method = [&](double t0, double tf) {
        return json{{"initial_temperature", t0},
                    {"final_temperature", tf},
                    {"expected_acceptance_at_initial", expectedAcceptance(worse, t0)},
                    {"expected_acceptance_at_final", expectedAcceptance(worse, tf)},
                    {"cooling_rate", coolingRate(t0, tf, sweeps, population_size)}};
    };
    return {{"target_acceptance", {{"initial", kInitialAcceptance}, {"final", kFinalAcceptance}}},
            {"cooling_rate_for_sweeps", sweeps},
            {"mean_delta_method", method(t0_mean, tf_mean)},
            {"distribution_method", method(t0_dist, tf_dist)}};
}

// calibration_deltas.csv と calibration_summary.json を書き、summary を返す
// 1周（全員分の推論1ジョブ）にかかる時間から、周回数ごとの所要日数を見積もる
json timingJson(const CalibrationResult& result, int sweeps) {
    std::vector<double> secs;
    if (result.initial_elapsed_sec >= 0) secs.push_back(result.initial_elapsed_sec);
    for (const auto& r : result.rounds) {
        if (r.elapsed_sec >= 0) secs.push_back(r.elapsed_sec);
    }
    json timing = {{"initial_job_sec", result.initial_elapsed_sec}, {"round_job_sec", json::array()}};
    for (const auto& r : result.rounds) timing["round_job_sec"].push_back(r.elapsed_sec);
    if (secs.empty()) return timing;
    double sum = 0.0;
    for (double s : secs) sum += s;
    const double per_sweep = sum / static_cast<double>(secs.size());
    timing["mean_sec_per_sweep"] = per_sweep;
    json estimate = json::object();
    for (int s : {50, 100, sweeps}) estimate[std::to_string(s)] = {{"hours", per_sweep * s / 3600.0},
                                                                    {"days", per_sweep * s / 86400.0}};
    timing["estimated_total_for_sweeps"] = estimate;
    return timing;
}

json writeOutputs(const BatchOptimizerConfig& c, int rounds, const std::string& out_dir,
                  const CalibrationResult& result, int population_size) {
    std::ostringstream csv;
    csv << "round,client_id,sweep,job_id,person_id,delta,skipped\n" << std::setprecision(17);
    std::vector<double> all;
    int all_skipped = 0;
    json per_round = json::array();
    for (const auto& r : result.rounds) {
        std::vector<double> deltas;
        int skipped = 0;
        for (size_t i = 0; i < r.person_ids.size(); ++i) {
            csv << r.round << "," << r.client_id << "," << r.sweep << "," << r.job_id << "," << r.person_ids[i]
                << "," << r.deltas[i] << "," << (r.skipped[i] ? 1 : 0) << "\n";
            if (r.skipped[i]) {
                ++skipped;
            } else {
                deltas.push_back(r.deltas[i]);
            }
        }
        all.insert(all.end(), deltas.begin(), deltas.end());
        all_skipped += skipped;
        per_round.push_back({{"round", r.round}, {"client_id", r.client_id}, {"sweep", r.sweep},
                             {"job_id", r.job_id}, {"elapsed_sec", r.elapsed_sec},
                             {"n_unparsed", r.n_unparsed}, {"n_length", r.n_length},
                             {"stats", statsJson(summarizeDeltas(deltas, skipped))}});
    }
    writeFileAtomic(fs::path(out_dir) / "calibration_deltas.csv", csv.str());

    const DeltaStats total = summarizeDeltas(all, all_skipped);
    json summary = {
        {"saved_at", timeString()},
        {"config", {{"client_id", c.client_id},
                    {"enable_thinking", c.enable_thinking},
                    {"reasoning_effort", c.enable_thinking ? json(c.reasoning_effort) : json(nullptr)},
                    {"max_tokens", c.max_tokens},
                    {"seed", c.seed},
                    {"mutation_step_size", c.mutation_step_size},
                    {"sweeps", c.sweeps},
                    {"system_prompt_path", c.system_prompt_path},
                    {"user_prompt_path", c.user_prompt_path},
                    {"population_csv", c.population_csv},
                    {"questions_csv", c.questions_csv},
                    {"real_ratios_csv", c.real_ratios_csv}}},
        {"rounds_requested", rounds},
        {"initial_tae", result.initial_tae},
        {"initial_job_id", result.initial_job_id},
        {"timing", timingJson(result, c.sweeps)},
        {"rounds", per_round},
        {"total", statsJson(total)},
    };
    const std::vector<double> worse = worseDeltas(all);
    if (!worse.empty()) summary["temperatures"] = temperaturesJson(worse, total.mean_worse, c.sweeps, population_size);
    writeFileAtomic(fs::path(out_dir) / "calibration_summary.json", summary.dump(2));
    return summary;
}

void printRound(const CalibrationRound& r, int rounds) {
    std::vector<double> deltas;
    int skipped = 0;
    for (size_t i = 0; i < r.deltas.size(); ++i) {
        if (r.skipped[i]) {
            ++skipped;
        } else {
            deltas.push_back(r.deltas[i]);
        }
    }
    const DeltaStats s = summarizeDeltas(deltas, skipped);
    std::cout << "[round " << r.round << "/" << rounds << "] job " << r.job_id << " (" << r.client_id << " sweep "
              << r.sweep << "): worse=" << s.n_worse << " better=" << s.n_better << " zero=" << s.n_zero
              << " skipped=" << s.n_skipped << " mean dE+=" << std::setprecision(6) << s.mean_worse
              << " unparsed=" << r.n_unparsed << " length=" << r.n_length << " inference="
              << std::fixed << std::setprecision(0) << r.elapsed_sec << "s" << std::defaultfloat << std::endl;
}

void printTemperatures(const json& summary) {
    const auto& total = summary.at("total");
    std::cout << "\n=== Calibration finished ===\n"
              << "initial TAE=" << std::setprecision(10) << summary.at("initial_tae").get<double>() << "\n"
              << "proposals=" << total.at("n_proposals") << " worse=" << total.at("n_worse")
              << " better=" << total.at("n_better") << " zero=" << total.at("n_zero")
              << " skipped=" << total.at("n_skipped") << "\n"
              << "mean dE+ (worse only)=" << std::setprecision(6) << total.at("mean_worse").get<double>() << "\n";
    const auto& timing = summary.at("timing");
    if (timing.contains("mean_sec_per_sweep")) {
        std::cout << "inference time per sweep (403 persons x " << "all questions, one job): " << std::fixed
                  << std::setprecision(0) << timing.at("mean_sec_per_sweep").get<double>() << "s ("
                  << std::setprecision(2) << timing.at("mean_sec_per_sweep").get<double>() / 3600.0 << " h)\n";
        for (const auto& [sweeps, e] : timing.at("estimated_total_for_sweeps").items()) {
            std::cout << "  " << sweeps << " sweeps: " << std::setprecision(1) << e.at("hours").get<double>()
                      << " h (" << e.at("days").get<double>() << " days)\n";
        }
        std::cout << std::defaultfloat;
    }
    if (!summary.contains("temperatures")) {
        std::cout << "no worse proposals; temperatures cannot be computed" << std::endl;
        return;
    }
    const auto& t = summary.at("temperatures");
    for (const char* name : {"mean_delta_method", "distribution_method"}) {
        const auto& m = t.at(name);
        std::cout << std::string(name) << ": T0=" << std::setprecision(6) << m.at("initial_temperature").get<double>()
                  << " Tf=" << m.at("final_temperature").get<double>()
                  << " (expected muAG " << std::setprecision(3)
                  << 100 * m.at("expected_acceptance_at_initial").get<double>() << "% -> "
                  << 100 * m.at("expected_acceptance_at_final").get<double>() << "%)"
                  << " cooling_rate(" << t.at("cooling_rate_for_sweeps") << " sweeps)=" << std::setprecision(10)
                  << m.at("cooling_rate").get<double>() << "\n";
    }
    std::cout << std::flush;
}

} // namespace

CalibrationResult runCalibration(const BatchOptimizerConfig& c, int rounds, const std::string& out_dir,
                                 JobClient& client, const std::function<void(int)>& sleep_arg) {
    const std::function<void(int)> sleep_sec =
        sleep_arg ? sleep_arg : [](int s) { std::this_thread::sleep_for(std::chrono::seconds(s)); };
    if (rounds < 1) throw std::invalid_argument("rounds must be at least 1");
    if (fs::weakly_canonical(out_dir) == fs::weakly_canonical(c.run_dir)) {
        throw std::invalid_argument("out_dir must differ from the optimization run_dir: " + out_dir);
    }

    const OptimizerInputs in = loadOptimizerInputs(c);
    const std::vector<std::string> qids = questionIds(in.questions);
    const int n = static_cast<int>(in.population.size());
    fs::create_directories(out_dir);
    writeFileAtomic(fs::path(out_dir) / ("server_info_" + std::to_string(std::time(nullptr)) + ".json"),
                    json::parse(client.info()).dump(2));

    // 初期状態：BatchOptimizer の sweep 0 と同じ手順（同じ seed から個性を決め、同じ client_id・sweep で投げる）
    std::mt19937_64 rng(c.seed);
    std::vector<Person> population = in.population;
    assignInitialPersonality(population, rng);
    const auto initial_prompts = buildPrompts(population, in.questions, in.system_template, in.user_template);
    const JobStatus initial = runJob(c, client, c.client_id, 0, initial_prompts, sleep_sec);
    const ParsedResults initial_parsed = parseResults(initial.results, initial_prompts, in.questions);
    TaeTracker tracker;
    tracker.setRealData(in.real_ratios);
    tracker.initialize(initial_parsed.answers, n, qids);

    // サーバーでの推論時間（開始から終了まで）。全員分を1ジョブで流したときの1周の時間になる
    auto elapsedOf = [&](const std::string& client_id, int sweep) {
        auto meta = client.findMeta(client_id, sweep);
        return meta && meta->started_at && meta->finished_at ? *meta->finished_at - *meta->started_at : -1.0;
    };

    CalibrationResult result;
    result.initial_tae = tracker.total();
    result.initial_job_id = initial.job_id;
    result.initial_elapsed_sec = elapsedOf(c.client_id, 0);
    json summary;
    writePopulationCsv((fs::path(out_dir) / "initial_population.csv").string(), population, initial_parsed.answers,
                       qids);
    std::cout << "[initial] job " << initial.job_id << " (" << c.client_id << " sweep 0): TAE=" << std::setprecision(10)
              << result.initial_tae << " unparsed=" << initial_parsed.n_unparsed
              << " length=" << initial_parsed.n_length << " inference=" << std::fixed << std::setprecision(0)
              << result.initial_elapsed_sec << "s" << std::defaultfloat << std::endl;

    for (int round = 1; round <= rounds; ++round) {
        // 毎回、初期状態の個性から変更案を作る（乱数は続きから引く。round 1 は本番の sweep 1 と同じ変更案）
        const SweepPlan plan = planSweep(population, c.mutation_step_size, rng);
        const auto prompts = buildPrompts(plan.proposals, in.questions, in.system_template, in.user_template);
        CalibrationRound r;
        r.round = round;
        r.client_id = round == 1 ? c.client_id : c.client_id + "-calib";
        r.sweep = round;
        const JobStatus st = runJob(c, client, r.client_id, r.sweep, prompts, sleep_sec);
        const ParsedResults parsed = parseResults(st.results, prompts, in.questions);
        r.job_id = st.job_id;
        r.elapsed_sec = elapsedOf(r.client_id, r.sweep);
        r.n_unparsed = parsed.n_unparsed;
        r.n_length = parsed.n_length;

        // 1人ずつ「初期状態でその人の回答だけを入れ替えた場合」の ΔTAE。tracker は初期状態のまま変えない
        for (const auto& person : population) {
            const int pid = person.person_id;
            r.person_ids.push_back(pid);
            auto it = parsed.answers.find(pid);
            const bool skip = it == parsed.answers.end() || it->second.empty();
            r.skipped.push_back(skip);
            const auto old_it = initial_parsed.answers.find(pid);
            const Answers empty;
            const Answers& old_answers = old_it == initial_parsed.answers.end() ? empty : old_it->second;
            r.deltas.push_back(skip ? 0.0 : tracker.evaluate(old_answers, it->second) - tracker.total());
        }
        result.rounds.push_back(r);
        printRound(r, rounds);
        // 途中で止まっても、そこまでの結果が残るように毎回書く
        summary = writeOutputs(c, rounds, out_dir, result, n);
    }
    printTemperatures(summary);
    return result;
}
