//
// Created by bp22029 on 2025/11/08.
//
// individual_response_manager_test.cpp
#include "gtest/gtest.h"  // GoogleTestのメインヘッダー
#include "individual_response_manager.hpp"  // テスト対象のヘッダー
#include "person.hpp"
#include "question.hpp"
#include "response_parser.hpp"
#include <vector>
#include <string>
TEST(IndividualResponseManagerTest, RecordResponseValidChoice) {
    // Arrange
    IndividualResponseManager manager;
    int test_person_id = 1001;
    std::string test_question_id = "TQ_1";
    int choice_to_record = 2; // 「2」を記録することを明示

    // Act
    // テスト対象の関数（recordResponse）を、準備した値で呼び出す
    manager.recordResponse(test_person_id, test_question_id, choice_to_record);

    // 戻り値（振る舞い）を取得
    IndividualResponse response = manager.getPersonResponses(test_person_id);

    // Assert
    auto it = response.responses.find(test_question_id);
    ASSERT_NE(it, response.responses.end());      // キーが存在すること
    EXPECT_EQ(it->second, choice_to_record); // 記録した値(2)と一致すること
}

TEST(IndividualResponseManagerTest, RecordMultipleQuestions) {
    // Arrange
    IndividualResponseManager manager;
    int test_person_id = 1001;

    // Act
    manager.recordResponse(test_person_id, "TQ_1", 2);
    manager.recordResponse(test_person_id, "TQ_2", 4); // 2つ目の質問を記録

    // Assert
    IndividualResponse response = manager.getPersonResponses(test_person_id);

    // TQ_1 の検証
    auto it1 = response.responses.find("TQ_1");
    ASSERT_NE(it1, response.responses.end());
    EXPECT_EQ(it1->second, 2);

    // TQ_2 の検証
    auto it2 = response.responses.find("TQ_2");
    ASSERT_NE(it2, response.responses.end());
    EXPECT_EQ(it2->second, 4);
}


TEST(IndividualResponseManagerTest, RecordOverwriteChoice) {
    // Arrange
    IndividualResponseManager manager;
    int test_person_id = 1001;
    std::string test_question_id = "TQ_1";

    // Act
    manager.recordResponse(test_person_id, test_question_id, 2); // 1回目の回答
    manager.recordResponse(test_person_id, test_question_id, 5); // 2回目（上書き）の回答

    // Assert
    IndividualResponse response = manager.getPersonResponses(test_person_id);
    auto it = response.responses.find(test_question_id);

    ASSERT_NE(it, response.responses.end());
    EXPECT_EQ(it->second, 5); // 5に上書きされていることを期待
    EXPECT_EQ(response.responses.size(), 1); // 回答が1件であることも確認
}

TEST(IndividualResponseManagerTest, GetNonExistentPerson) {
    // Arrange
    IndividualResponseManager manager;

    // Act (getPersonResponses)
    IndividualResponse response = manager.getPersonResponses(9999); // 存在しないID

    // Assert (getPersonResponses)
    // person_id は要求されたIDで、responses マップは空であること
    EXPECT_EQ(response.person_id, 9999);
    EXPECT_TRUE(response.responses.empty());

    // Act (findPersonResponses)
    const IndividualResponse* pResponse = manager.findPersonResponses(9999);

    // Assert (findPersonResponses)
    EXPECT_EQ(pResponse, nullptr); // nullptr が返ることを期待
}