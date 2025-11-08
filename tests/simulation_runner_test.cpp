#include "gtest/gtest.h"
#include "simulation_runner.hpp"
#include "individual_response_manager.hpp"
#include "person.hpp"
#include "question.hpp"

// ★「偽物」の関数を定義（モック）
std::string MockQueryFunction(
    const std::string& prompt,
    const std::string& host,
    int port,
    const LLMParams& params)
{
    // プロンプトに "Q1" が含まれていたら "1" を返す
    if (prompt.find("Q1") != std::string::npos) {
        return "1";
    }
    // プロンプトに "Q2" が含まれていたら "3" を返す
    if (prompt.find("Q2") != std::string::npos) {
        return "3";
    }
    return "99"; // その他
}

TEST(SimulationRunnerTest, RunSimulationWithMock) {
    // 1. 準備 (Arrange)
    // ダミーのデータを作成
    std::vector<Person> population = {
        {100, "男性"} // Person 1人
    };
    std::vector<Question> questions = {
        {"q1", "Q1: Test Question 1", {"Choice1", "Choice2"}},
        {"q2", "Q2: Test Question 2", {"A", "B", "C"}}
    };
    std::string sys_tpl = "SYS";
    std::string user_tpl = "{質問}";
    std::vector<SurveyResult> results; // (現在は使われていないが引数のために用意)

    IndividualResponseManager manager; // ★ 検証用のマネージャー

    // 2. 実行 (Act)
    // ★ runSurveySimulation に「偽物」の関数（MockQueryFunction）を渡す
    runSurveySimulation(
        population,
        questions,
        sys_tpl,
        user_tpl,
        results,
        manager,           // ★ 検証用マネージャーを渡す
        &MockQueryFunction // ★ 偽物の関数を渡す
    );

    // 3. 検証 (Assert)
    // manager の内部状態をチェック
    const IndividualResponse* pResponse = manager.findPersonResponses(100);
    ASSERT_NE(pResponse, nullptr); // Person 100 が記録されていること

    // Q1 の回答が、モックが返した "1" になっているか
    auto it1 = pResponse->responses.find("q1");
    ASSERT_NE(it1, pResponse->responses.end());
    EXPECT_EQ(it1->second, 1);

    // Q2 の回答が、モックが返した "3" になっているか
    auto it2 = pResponse->responses.find("q2");
    ASSERT_NE(it2, pResponse->responses.end());
    EXPECT_EQ(it2->second, 3);
}