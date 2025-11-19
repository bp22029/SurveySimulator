#include "gtest/gtest.h"
#include "simulation_runner.hpp"
#include "individual_response_manager.hpp"
#include "person.hpp"
#include "question.hpp"
// LLMResponseの定義を知るために必要（定義されているヘッダーをインクルードしてください）
#include "../include/llm_client.hpp"

// ★「偽物」の関数を定義（モック）
// 戻り値を std::string から LLMResponse に変更
LLMResponse MockQueryFunction(
    const std::string& prompt,
    const std::string& host,
    int port,
    const LLMParams& params)
{
    LLMResponse res;
    res.success = true; // テストなので通信成功とする
    res.reasoning_content = "これはテスト用のモック思考プロセスです。"; // ダミーの思考

    // プロンプトに "Q1" が含まれていたら "1" を返す
    if (prompt.find("Q1") != std::string::npos) {
        res.content = "1";
        return res;
    }
    // プロンプトに "Q2" が含まれていたら "3" を返す
    if (prompt.find("Q2") != std::string::npos) {
        res.content = "3";
        return res;
    }

    res.content = "99"; // その他
    return res;
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
    std::vector<SurveyResult> results;

    IndividualResponseManager manager; // ★ 検証用のマネージャー

    // 2. 実行 (Act)
    // ★ runSurveySimulation に「偽物」の関数（MockQueryFunction）を渡す
    runSurveySimulation(
        population,
        questions,
        sys_tpl,
        user_tpl,
        results,
        manager,           // ★ 検証用マネージャー
        &MockQueryFunction, // ★ 偽物の関数（型が合うようになりました）
        "test_dummy_log.txt" // ★追加: テスト用のログファイル名
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