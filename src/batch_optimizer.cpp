#include "../include/batch_optimizer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include "nlohmann/json.hpp"
#include "../include/data_loader.hpp"
#include "../include/portable_random.hpp"
#include "../include/prompt_generator.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

void updateScores(Person& p) {
    p.personality.neuroticism.updateScore();
    p.personality.conscientiousness.updateScore();
    p.personality.extraversion.updateScore();
    p.personality.agreeableness.updateScore();
    p.personality.openness.updateScore();
}

float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

std::string nowString() {
    std::time_t t = std::time(nullptr);
    std::ostringstream os;
    os << std::put_time(std::localtime(&t), "%Y-%m-%dT%H:%M:%S");
    return os.str();
}

std::string sweepTag(int sweep) {
    std::ostringstream os;
    os << std::setw(3) << std::setfill('0') << sweep;
    return os.str();
}

void writeFileAtomic(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out) throw std::runtime_error("cannot write " + tmp.string());
        out << content;
        if (!out) throw std::runtime_error("failed to write " + tmp.string());
    }
    fs::rename(tmp, path);
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

std::vector<std::string> questionIds(const std::vector<Question>& questions) {
    std::vector<std::string> ids;
    for (const auto& q : questions) ids.push_back(q.id);
    return ids;
}

} // namespace

// ==========================================
// 設定
// ==========================================

BatchOptimizerConfig loadBatchOptimizerConfig(const std::string& path) {
    const json j = json::parse(readFile(path));
    BatchOptimizerConfig c;
    c.server_url = j.at("server_url").get<std::string>();
    c.client_id = j.at("client_id").get<std::string>();
    c.sweeps = j.at("sweeps").get<int>();
    c.initial_temperature = j.at("initial_temperature").get<double>();
    c.final_temperature = j.at("final_temperature").get<double>();
    c.mutation_step_size = j.at("mutation_step_size").get<double>();
    c.seed = j.at("seed").get<uint64_t>();
    c.population_csv = j.at("population_csv").get<std::string>();
    c.questions_csv = j.at("questions_csv").get<std::string>();
    c.system_prompt_path = j.at("system_prompt_path").get<std::string>();
    c.user_prompt_path = j.at("user_prompt_path").get<std::string>();
    c.real_ratios_csv = j.at("real_ratios_csv").get<std::string>();
    c.run_dir = j.at("run_dir").get<std::string>();
    c.poll_interval_sec = j.value("poll_interval_sec", 30);
    c.max_job_attempts = j.value("max_job_attempts", 3);
    return c;
}

void requireLfInputFiles(const BatchOptimizerConfig& c) {
    for (const auto& path : {c.population_csv, c.questions_csv, c.system_prompt_path, c.user_prompt_path,
                             c.real_ratios_csv}) {
        if (readFile(path).find('\r') != std::string::npos) {
            throw std::runtime_error(
                path + " has CRLF line endings. Convert it to LF (e.g. `sed -i 's/\\r$//' " + path +
                "`); CR characters would end up in the prompts and change the model outputs.");
        }
    }
}

// 最適化の途中で変えてはいけない設定（設計書 §6）
static json fixedSettings(const BatchOptimizerConfig& c) {
    return {
        {"client_id", c.client_id},
        {"sweeps", c.sweeps},
        {"initial_temperature", c.initial_temperature},
        {"final_temperature", c.final_temperature},
        {"mutation_step_size", c.mutation_step_size},
        {"seed", c.seed},
        {"population_csv", c.population_csv},
        {"questions_csv", c.questions_csv},
        {"system_prompt_path", c.system_prompt_path},
        {"user_prompt_path", c.user_prompt_path},
        {"real_ratios_csv", c.real_ratios_csv},
    };
}

// ==========================================
// 1周の中身
// ==========================================

std::vector<float*> facetPointers(Person& p) {
    auto& b = p.personality;
    // 昨年の mutatePerson / randomBFI2 と同じ順番
    return {
        &b.neuroticism.anxiety, &b.neuroticism.depression, &b.neuroticism.emotional_volatility,
        &b.conscientiousness.organization, &b.conscientiousness.productivity, &b.conscientiousness.responsibility,
        &b.extraversion.sociability, &b.extraversion.assertiveness, &b.extraversion.energy_level,
        &b.agreeableness.compassion, &b.agreeableness.respectfulness, &b.agreeableness.trust,
        &b.openness.intellectual_curiosity, &b.openness.aesthetic_sensitivity, &b.openness.creative_imagination,
    };
}

std::vector<float> facetValues(const Person& person) {
    Person copy = person;
    std::vector<float> values;
    for (float* f : facetPointers(copy)) values.push_back(*f);
    return values;
}

void setFacetValues(Person& person, const std::vector<float>& values) {
    auto facets = facetPointers(person);
    if (values.size() != facets.size()) throw std::runtime_error("setFacetValues: expected 15 values");
    for (size_t i = 0; i < facets.size(); ++i) *facets[i] = values[i];
    updateScores(person);
}

void assignInitialPersonality(std::vector<Person>& population, std::mt19937_64& rng) {
    for (auto& person : population) {
        for (float* f : facetPointers(person)) *f = static_cast<float>(portable_random::uniformReal(rng));
        updateScores(person);
    }
}

Person perturbPerson(const Person& origin, double step_size, std::mt19937_64& rng) {
    Person mutated = origin;
    for (float* f : facetPointers(mutated)) {
        *f = clamp01(*f + static_cast<float>(portable_random::normal(rng, 0.0, step_size)));
    }
    updateScores(mutated);
    return mutated;
}

SweepPlan planSweep(const std::vector<Person>& population, double step_size, std::mt19937_64& rng) {
    SweepPlan plan;
    plan.order.resize(population.size());
    for (size_t i = 0; i < population.size(); ++i) plan.order[i] = static_cast<int>(i);
    portable_random::shuffle(plan.order, rng);

    plan.proposals = population;
    for (int idx : plan.order) plan.proposals[idx] = perturbPerson(population[idx], step_size, rng);
    return plan;
}

std::vector<JobPrompt> buildPrompts(const std::vector<Person>& persons, const std::vector<Question>& questions,
                                    const std::string& system_template, const std::string& user_template) {
    std::vector<JobPrompt> prompts;
    prompts.reserve(persons.size() * questions.size());
    for (const auto& person : persons) {
        for (const auto& q : questions) {
            prompts.push_back({
                std::to_string(person.person_id) + "_" + q.id,
                generatePrompt(system_template, person, q),
                generatePrompt(user_template, person, q),
            });
        }
    }
    return prompts;
}

int extractFinalAnswer(const std::string& response) {
    // 思考の中に下書きの <answer> が出てくることがあるので、思考の後だけを見る
    const std::string end_think = "</think>";
    size_t pos = response.rfind(end_think);
    std::string tail = pos == std::string::npos ? response : response.substr(pos + end_think.size());

    static const std::regex re(R"(<answer>\s*(\d+)\s*</answer>)");
    int answer = -1;
    for (auto it = std::sregex_iterator(tail.begin(), tail.end(), re); it != std::sregex_iterator(); ++it) {
        try {
            answer = std::stoi((*it)[1].str());
        } catch (...) {
            answer = -1;
        }
    }
    return answer;
}

ParsedResults parseResults(const std::vector<JobOutput>& outputs, const std::vector<JobPrompt>& prompts,
                           const std::vector<Question>& questions) {
    std::set<std::string> expected;
    for (const auto& p : prompts) expected.insert(p.id);
    std::set<std::string> seen;
    for (const auto& o : outputs) {
        if (!expected.count(o.id) || !seen.insert(o.id).second) {
            throw std::runtime_error("unexpected or duplicate result id: " + o.id);
        }
    }
    if (seen.size() != expected.size()) {
        throw std::runtime_error("missing results: expected " + std::to_string(expected.size()) +
                                 ", got " + std::to_string(seen.size()));
    }

    std::map<std::string, size_t> n_choices;
    for (const auto& q : questions) n_choices[q.id] = q.choices.size();

    ParsedResults parsed;
    for (const auto& o : outputs) {
        size_t sep = o.id.find('_');
        int person_id = std::stoi(o.id.substr(0, sep));
        std::string qid = o.id.substr(sep + 1);
        parsed.answers[person_id]; // 1件も解析できなかった人も空の回答として載せる

        if (o.finish_reason == "length") {
            // 途中で打ち切られた出力は最終回答とみなさない
            parsed.n_length++;
            parsed.n_unparsed++;
            continue;
        }
        int choice = extractFinalAnswer(o.response);
        if (choice < 1 || choice > static_cast<int>(n_choices[qid])) {
            parsed.n_unparsed++;
            continue;
        }
        parsed.answers[person_id][qid] = choice;
    }
    return parsed;
}

double coolingRate(double initial_temperature, double final_temperature, int sweeps, int population_size) {
    return std::pow(final_temperature / initial_temperature,
                    1.0 / (static_cast<double>(sweeps) * population_size));
}

std::vector<Decision> applySweep(const SweepPlan& plan, const ParsedResults& parsed,
                                 std::vector<Person>& population, std::map<int, Answers>& answers,
                                 TaeTracker& tracker, double& temperature, double alpha,
                                 std::mt19937_64& rng) {
    std::vector<Decision> decisions;
    decisions.reserve(plan.order.size());
    for (size_t pos = 0; pos < plan.order.size(); ++pos) {
        const int idx = plan.order[pos];
        const int person_id = population[idx].person_id;
        Decision d{static_cast<int>(pos), person_id, 0.0, false, false, temperature, tracker.total()};

        auto it = parsed.answers.find(person_id);
        if (it == parsed.answers.end() || it->second.empty()) {
            d.skipped = true;
        } else {
            const Answers& new_answers = it->second;
            Answers& current = answers[person_id];
            d.delta = tracker.evaluate(current, new_answers) - tracker.total();
            // dE < 0 のときは乱数を引かない（設計書 §4.1）
            d.accepted = d.delta < 0 || portable_random::uniformReal(rng) < std::exp(-d.delta / temperature);
            if (d.accepted) {
                tracker.commit(current, new_answers);
                for (const auto& [qid, choice] : new_answers) current[qid] = choice;
                population[idx] = plan.proposals[idx];
            }
            d.tae = tracker.total();
        }
        decisions.push_back(d);
        temperature *= alpha; // 冷却は1件ごと
    }
    return decisions;
}

// ==========================================
// チェックポイントと出力
// ==========================================

namespace {

struct RunState {
    int completed_sweep = -1;     // -1: 未開始、0: 初期個性での推論まで完了
    std::vector<Person> population;
    std::map<int, Answers> answers;
    double temperature = 0.0;
    double alpha = 0.0;
    std::mt19937_64 rng;
    json job_ids = json::object();
    json history = json::array();
    json server = json::object();  // 最初に記録したサーバー設定
};

fs::path checkpointPath(const BatchOptimizerConfig& c) { return fs::path(c.run_dir) / "checkpoint.json"; }

void saveCheckpoint(const BatchOptimizerConfig& c, const RunState& s, const TaeTracker& tracker) {
    json personality = json::array();
    for (const auto& p : s.population) personality.push_back({{"person_id", p.person_id}, {"facets", facetValues(p)}});
    json answers = json::object();
    for (const auto& [pid, a] : s.answers) answers[std::to_string(pid)] = a;

    json j = {
        {"format", 1},
        {"saved_at", nowString()},
        {"settings", fixedSettings(c)},
        {"completed_sweep", s.completed_sweep},
        {"temperature", s.temperature},
        {"alpha", s.alpha},
        {"tae", tracker.total()},
        {"rng_state", portable_random::saveState(s.rng)},
        {"job_ids", s.job_ids},
        {"server", s.server},
        {"history", s.history},
        {"personality", personality},
        {"answers", answers},
    };
    writeFileAtomic(checkpointPath(c), j.dump(1));
}

// 戻り値: チェックポイントがあったか
bool loadCheckpoint(const BatchOptimizerConfig& c, RunState& s, const std::vector<Person>& base_population) {
    if (!fs::exists(checkpointPath(c))) return false;
    const json j = json::parse(readFile(checkpointPath(c).string()));
    if (j.at("settings") != fixedSettings(c)) {
        throw std::runtime_error("config differs from the checkpoint in " + c.run_dir +
                                 ". Settings must not change during an optimization.\ncheckpoint: " +
                                 j.at("settings").dump() + "\nconfig:     " + fixedSettings(c).dump());
    }
    s.completed_sweep = j.at("completed_sweep").get<int>();
    s.temperature = j.at("temperature").get<double>();
    s.alpha = j.at("alpha").get<double>();
    portable_random::loadState(s.rng, j.at("rng_state").get<std::string>());
    s.job_ids = j.at("job_ids");
    s.server = j.at("server");
    s.history = j.at("history");

    s.population = base_population;
    const auto& personality = j.at("personality");
    if (personality.size() != s.population.size()) throw std::runtime_error("checkpoint population size mismatch");
    for (size_t i = 0; i < s.population.size(); ++i) {
        if (personality[i].at("person_id").get<int>() != s.population[i].person_id) {
            throw std::runtime_error("checkpoint person order mismatch at index " + std::to_string(i));
        }
        setFacetValues(s.population[i], personality[i].at("facets").get<std::vector<float>>());
    }
    for (const auto& [pid, a] : j.at("answers").items()) s.answers[std::stoi(pid)] = a.get<Answers>();
    return true;
}

} // namespace

void writePopulationCsv(const std::string& path, const std::vector<Person>& population,
                        const std::map<int, Answers>& answers, const std::vector<std::string>& question_ids) {
    std::ostringstream os;
    os << "person_id,gender,prefecture_name,city_name,age,industry_type,employment_type,company_size,"
       << "family_type,role_household_type,total_income,"
       << "extraversion,sociability,assertiveness,energy_level,"
       << "agreeableness,compassion,respectfulness,trust,"
       << "conscientiousness,organization,productivity,responsibility,"
       << "neuroticism,anxiety,depression,emotional_volatility,"
       << "openness,intellectual_curiosity,aesthetic_sensitivity,creative_imagination";
    for (const auto& qid : question_ids) os << "," << qid;
    os << "\n";

    for (const auto& p : population) {
        const auto& b = p.personality;
        os << p.person_id << "," << p.gender << "," << p.prefecture_name << "," << p.city_name << "," << p.age << ","
           << p.industry_type << "," << p.employment_type << "," << p.company_size << "," << p.family_type << ","
           << p.role_household_type << "," << p.total_income << ","
           << b.extraversion.score << "," << b.extraversion.sociability << "," << b.extraversion.assertiveness << ","
           << b.extraversion.energy_level << ","
           << b.agreeableness.score << "," << b.agreeableness.compassion << "," << b.agreeableness.respectfulness << ","
           << b.agreeableness.trust << ","
           << b.conscientiousness.score << "," << b.conscientiousness.organization << ","
           << b.conscientiousness.productivity << "," << b.conscientiousness.responsibility << ","
           << b.neuroticism.score << "," << b.neuroticism.anxiety << "," << b.neuroticism.depression << ","
           << b.neuroticism.emotional_volatility << ","
           << b.openness.score << "," << b.openness.intellectual_curiosity << "," << b.openness.aesthetic_sensitivity
           << "," << b.openness.creative_imagination;
        auto it = answers.find(p.person_id);
        for (const auto& qid : question_ids) {
            os << ",";
            if (it == answers.end()) continue;
            auto a = it->second.find(qid);
            if (a != it->second.end()) os << a->second;
        }
        os << "\n";
    }
    writeFileAtomic(path, os.str());
}

namespace {

void writeSweepOutputs(const BatchOptimizerConfig& c, int sweep, const RunState& s,
                       const std::vector<Question>& questions, const std::vector<Decision>& decisions) {
    const fs::path dir = fs::path(c.run_dir) / "sweeps";
    fs::create_directories(dir);

    if (!decisions.empty()) {
        std::ostringstream os;
        os << "position,person_id,temperature,delta,accepted,skipped,tae\n" << std::setprecision(17);
        for (const auto& d : decisions) {
            os << d.position << "," << d.person_id << "," << d.temperature << "," << d.delta << ","
               << d.accepted << "," << d.skipped << "," << d.tae << "\n";
        }
        writeFileAtomic(dir / ("sweep_" + sweepTag(sweep) + "_decisions.csv"), os.str());
    }

    writePopulationCsv((dir / ("sweep_" + sweepTag(sweep) + "_population.csv")).string(), s.population, s.answers,
                       questionIds(questions));

    std::ostringstream os;
    os << "sweep,job_id,tae,temperature,accepted,skipped,n_unparsed,n_length,finished_at\n" << std::setprecision(17);
    for (const auto& h : s.history) {
        os << h.at("sweep") << "," << h.at("job_id") << "," << h.at("tae").get<double>() << ","
           << h.at("temperature").get<double>() << "," << h.at("accepted") << "," << h.at("skipped") << ","
           << h.at("n_unparsed") << "," << h.at("n_length") << "," << h.at("finished_at").get<std::string>() << "\n";
    }
    writeFileAtomic(fs::path(c.run_dir) / "sweeps.csv", os.str());
}

// 再現性に関わるサーバー設定（git の commit は含めない。サーバーと無関係な commit でも変わるため）
json reproducibleServerSettings(const json& info) {
    json out = json::object();
    for (const char* key : {"model", "revision", "packages", "env", "torch_cuda", "gpu"}) {
        if (info.contains(key)) out[key] = info[key];
    }
    if (info.contains("engine")) {
        for (const char* key : {"llm", "sampling", "enable_thinking"}) {
            if (info["engine"].contains(key)) out["engine"][key] = info["engine"][key];
        }
    }
    return out;
}

JobStatus runJob(const BatchOptimizerConfig& c, JobClient& client, int sweep, const std::vector<JobPrompt>& prompts,
                 const std::function<void(int)>& sleep_sec) {
    if (auto existing = client.find(c.client_id, sweep)) {
        std::cout << "[sweep " << sweep << "] found existing job " << *existing << " on the server" << std::endl;
    }
    // 既存のジョブがあっても投げる。サーバーは同じ内容なら既存の job_id を返し、違えば 409 になるので、
    // 再開時に変更案が前回と同じであることの確認にもなる
    int job_id = client.submit(c.client_id, sweep, prompts);
    std::cout << "[sweep " << sweep << "] job " << job_id << " (" << prompts.size() << " prompts)" << std::endl;

    int failures = 0;
    while (true) {
        JobStatus st = client.get(job_id);
        if (st.client_id != c.client_id || st.sweep != sweep) {
            throw std::runtime_error("job " + std::to_string(job_id) + " belongs to client_id=" + st.client_id +
                                     " sweep=" + std::to_string(st.sweep));
        }
        if (st.status == "done") return st;
        if (st.status == "failed") {
            ++failures;
            std::cerr << "[sweep " << sweep << "] job " << job_id << " failed (" << failures << "/"
                      << c.max_job_attempts << "):\n" << st.error << std::endl;
            if (failures >= c.max_job_attempts) throw std::runtime_error("job failed too many times");
            client.submit(c.client_id, sweep, prompts); // failed のジョブは同じ job_id で再実行される
            continue;
        }
        sleep_sec(c.poll_interval_sec);
    }
}

} // namespace

// ==========================================
// 全体
// ==========================================

void runBatchOptimization(const BatchOptimizerConfig& c, JobClient& client,
                          const std::function<void(int)>& sleep_arg) {
    const std::function<void(int)> sleep_sec =
        sleep_arg ? sleep_arg : [](int s) { std::this_thread::sleep_for(std::chrono::seconds(s)); };

    requireLfInputFiles(c);
    const std::vector<Person> base_population = readSyntheticPopulation(c.population_csv);
    const std::vector<Question> questions = readQuestions(c.questions_csv);
    const std::string system_template = readPromptTemplate(c.system_prompt_path);
    const std::string user_template = readPromptTemplate(c.user_prompt_path);
    if (base_population.empty() || questions.empty() || system_template.empty() || user_template.empty()) {
        throw std::runtime_error("failed to load population, questions or prompt templates");
    }
    TaeTracker tracker;
    if (!tracker.loadRealData(c.real_ratios_csv)) throw std::runtime_error("failed to load " + c.real_ratios_csv);
    const int n = static_cast<int>(base_population.size());
    fs::create_directories(c.run_dir);

    // サーバー情報を記録し、最初の記録と食い違えば止める（設計書 §6）
    const json info = json::parse(client.info());
    writeFileAtomic(fs::path(c.run_dir) / ("server_info_" + std::to_string(std::time(nullptr)) + ".json"), info.dump(2));

    RunState s;
    if (loadCheckpoint(c, s, base_population)) {
        if (reproducibleServerSettings(info) != s.server) {
            throw std::runtime_error("server settings changed since this optimization started.\nbefore: " +
                                     s.server.dump() + "\nnow:    " + reproducibleServerSettings(info).dump());
        }
        tracker.initialize(s.answers, n, questionIds(questions));
        std::cout << "Resuming after sweep " << s.completed_sweep << " (TAE=" << std::setprecision(10)
                  << tracker.total() << ", T=" << s.temperature << ")" << std::endl;
    } else {
        // sweep 0: 初期個性を決めて、全員を推論する
        s.server = reproducibleServerSettings(info);
        s.rng.seed(c.seed);
        s.population = base_population;
        assignInitialPersonality(s.population, s.rng);
        s.alpha = coolingRate(c.initial_temperature, c.final_temperature, c.sweeps, n);
        s.temperature = c.initial_temperature;

        const auto prompts = buildPrompts(s.population, questions, system_template, user_template);
        const JobStatus st = runJob(c, client, 0, prompts, sleep_sec);
        const ParsedResults parsed = parseResults(st.results, prompts, questions);
        s.answers = parsed.answers;
        tracker.initialize(s.answers, n, questionIds(questions));
        s.completed_sweep = 0;
        s.job_ids["0"] = st.job_id;
        s.history.push_back({{"sweep", 0}, {"job_id", st.job_id}, {"tae", tracker.total()},
                             {"temperature", s.temperature}, {"accepted", 0}, {"skipped", 0},
                             {"n_unparsed", parsed.n_unparsed}, {"n_length", parsed.n_length},
                             {"finished_at", nowString()}});
        saveCheckpoint(c, s, tracker);
        writeSweepOutputs(c, 0, s, questions, {});
        std::cout << "[sweep 0] initial TAE=" << std::setprecision(10) << tracker.total()
                  << " unparsed=" << parsed.n_unparsed << " length=" << parsed.n_length << std::endl;
    }

    for (int sweep = s.completed_sweep + 1; sweep <= c.sweeps; ++sweep) {
        const SweepPlan plan = planSweep(s.population, c.mutation_step_size, s.rng);
        const auto prompts = buildPrompts(plan.proposals, questions, system_template, user_template);
        const JobStatus st = runJob(c, client, sweep, prompts, sleep_sec);
        const ParsedResults parsed = parseResults(st.results, prompts, questions);

        const auto decisions = applySweep(plan, parsed, s.population, s.answers, tracker, s.temperature, s.alpha, s.rng);
        const int accepted = static_cast<int>(std::count_if(decisions.begin(), decisions.end(),
                                                            [](const Decision& d) { return d.accepted; }));
        const int skipped = static_cast<int>(std::count_if(decisions.begin(), decisions.end(),
                                                           [](const Decision& d) { return d.skipped; }));
        s.completed_sweep = sweep;
        s.job_ids[std::to_string(sweep)] = st.job_id;
        s.history.push_back({{"sweep", sweep}, {"job_id", st.job_id}, {"tae", tracker.total()},
                             {"temperature", s.temperature}, {"accepted", accepted}, {"skipped", skipped},
                             {"n_unparsed", parsed.n_unparsed}, {"n_length", parsed.n_length},
                             {"finished_at", nowString()}});
        saveCheckpoint(c, s, tracker);
        writeSweepOutputs(c, sweep, s, questions, decisions);
        std::cout << "[sweep " << sweep << "/" << c.sweeps << "] TAE=" << std::setprecision(10) << tracker.total()
                  << " accepted=" << accepted << "/" << n << " skipped=" << skipped
                  << " unparsed=" << parsed.n_unparsed << " length=" << parsed.n_length
                  << " T=" << s.temperature << std::endl;
    }
    std::cout << "Optimization finished: " << c.sweeps << " sweeps, TAE=" << std::setprecision(10)
              << tracker.total() << std::endl;
}
