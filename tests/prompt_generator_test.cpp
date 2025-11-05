//
// Created by bp22029 on 2025/11/03.
//
#include "gtest/gtest.h"        // GoogleTestのメインヘッダー
#include "response_parser.hpp"  // テスト対象のヘッダー

// テストスイート名 "ResponseParserTest"、テストケース名 "ParseValidDigit"
TEST(ResponseParserTest, ParseValidDigit) {
    // "1" という文字列を渡したら、 1 という整数が返ってくることを期待する
    EXPECT_EQ(1, parseLlmAnswer("1"));
}