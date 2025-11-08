//
// Created by bp22029 on 2025/11/03.
//
#include "gtest/gtest.h"        // GoogleTestのメインヘッダー
#include "response_parser.hpp"  // テスト対象のヘッダー

// テストスイート名 "ResponseParserTest"、テストケース名 "ParseValidDigit"
TEST(ResponseParserTest, ParseValidDigit) {
    // "1" という文字列を渡したら、 1 という整数が返ってくることを期待する
    EXPECT_EQ(1, parseLlmAnswer("1"));
    EXPECT_EQ(5, parseLlmAnswer("5"));
}

TEST(ResponseParserTest,PsrseEmptyString){
    // 空文字列を渡したら、-1が返ってくることを期待する
    //　Arrange
    std::string expected_output = "";
    // Act
    int result = parseLlmAnswer(expected_output);
    // Assert
    EXPECT_EQ(-1,result);
}

//数字以外の異常系テスト
TEST(ResponseParserTest,ParseNonDigitString){
    // "abc" という文字列を渡したら、-1が返ってくることを期待する
    EXPECT_EQ(-1,parseLlmAnswer("abc"));
    EXPECT_EQ(-1,parseLlmAnswer("!@#"));
    EXPECT_EQ(-1,parseLlmAnswer("A"));
}

// 異常系テスト（数字で始まるが、余計な文字がある）
// ※現在の parseLlmAnswer の実装 は最初の1文字しか見ないため、"1a" は 1 として成功するはず。
TEST(ResponseParserTest, ParseDigitWithExtraChars) {
    EXPECT_EQ(1, parseLlmAnswer("1a"));
    EXPECT_EQ(2, parseLlmAnswer("2. そう思う"));
}