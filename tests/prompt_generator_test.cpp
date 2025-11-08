//
// Created by bp22029 on 2025/11/03.
//
#include "gtest/gtest.h"
#include "prompt_generator.hpp"
#include "person.hpp"
#include "question.hpp"

TEST(PromptGeneratorTest, BasicReplacement) {
    // 1. 準備 (Arrange)
    // テストコード内で完結するダミーデータ
    Person dummy_person;
    dummy_person.age = 30;
    dummy_person.gender = "男性";
    dummy_person.personality.extraversion.sociability = 0.8f;

    Question dummy_question;
    dummy_question.text = "テスト質問";
    dummy_question.choices = {"選択肢1", "選択肢2", "選択肢3"};

    std::string test_template =
        "年齢: {年齢}\n"
        "性別: {性別}\n"
        "社交性: {社交性}\n"
        "質問: {質問}\n"
        "{回答選択肢}";

    // 期待する「最終的な結果」
    std::string expected_output =
        "年齢: 30\n"
        "性別: 男性\n"
        "社交性: 0.80\n" // snprintf("%.2f") のフォーマットに注意
        "質問: テスト質問\n"
        "1. 選択肢1\n"
        "2. 選択肢2\n"
        "3. 選択肢3\n";

    // 2. 実行 (Act)
    std::string actual_output = generatePrompt(test_template, dummy_person, dummy_question);

    // 3. 検証 (Assert)
    EXPECT_EQ(expected_output, actual_output);
}