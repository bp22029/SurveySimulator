#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "../include/data_loader.hpp"
#include "../include/prompt_generator.hpp"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "llm_client.hpp"
#include "survey_results.hpp"

// nlohmann/jsonを使いやすくするために名前空間を指定
using json = nlohmann::json;

// 応答文字列を解析し、対応するSurveyResultのカウンターを更新する関数


int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    // 合成人口データの読み込み
    std::vector<Person> population = readSyntheticPopulation("../data/sample_synthetic_population.csv");
    if (population.empty()) {
        return 1;
    }

    // 質問データの読み込み
    std::vector<Question> questions = readQuestions("../data/ssm2015.csv");
    if (questions.empty()) {
        return 1;
    }

    // プロンプトテンプレートの読み込み
    std::string prompt_template = readPromptTemplate();

    //　回答集計の配列を初期化
    std::vector<SurveyResult> results;
    initializeSurveyResults(results,questions);

    // プロジェクトを書き込む変数を定義
    std::string generated_prompt;

    int total_simulations = population.size() * questions.size();
    int current_count = 0;
    for (const auto& person : population) {

        for (int i = 0; i < questions.size(); ++i) {
            current_count++;
            std::cout << "\n[" << current_count << "/" << total_simulations << "] "
                      << "Agent ID: " << person.id << ", Question ID: " << questions[i].id << std::endl;

            // プロンプト生成
            generated_prompt = generatePrompt(prompt_template, person, questions[i]);

            //　LLM問い合わせ、質問回答
            std::string content = queryLLM(generated_prompt);
            std::cout << content << std::endl;

            //　回答の解析と集計
            parseAndRecordAnswer(content, questions[i], results[i]);
        }
    }

    // 5. 集計結果のCSVファイルへの書き込み
    std::ofstream csv_file("../data/survey_results.csv");
    if (!csv_file.is_open()) {
        std::cerr << "CSVファイルのオープンに失敗しました。" << std::endl;
        return 1;
    }

    // ヘッダー行の書き込み (最大選択肢数を考慮)
    // ※簡単のため、ここでは最大5と仮定しますが、将来的には全質問を見て動的に決めるのがベストです
    csv_file << "question_id,full_question_text,choice_1_count,choice_2_count,choice_3_count,choice_4_count,choice_5_count\n";

    for (const auto& result : results) {
        // 質問IDと、"で囲んだ質問文を書き込む (改善点1)
        csv_file << result.question_id << ",\"" << result.question_text << "\"";

        // 選択肢の数だけループしてカウントを書き込む (改善点2)
        // ヘッダーに合わせて最大5列分を書き込む
        for (int i = 1; i <= 5; ++i) {
            csv_file << ","; // まずカンマを書き込む

            // result.answer_countsの中にキー(i)が存在するかチェック
            auto it = result.answer_counts.find(i);
            if (it != result.answer_counts.end()) {
                // キーが存在すれば、その値を書き込む
                csv_file << it->second; // it->second がカウント数
            }
            // キーが存在しなければ、何も書き込まない（空のセルになる）
        }
        csv_file << "\n";
    }

    csv_file.close();
    std::cout << "集計結果をCSVファイルに書き込みました: ../data/survey_results.csv" << std::endl;
    std::cout << "処理が完了しました。" << std::endl;



    return 0;
}