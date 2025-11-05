# (1) テスト実行ファイル "run_tests" を定義
add_executable(run_tests
        prompt_generator_test.cpp
        response_parser_test.cpp
)

# (2) run_tests が必要とするライブラリをリンク
target_link_libraries(run_tests PRIVATE
        SurveySimulatorLib  # (B)で定義したロジック本体
        GTest::gtest_main     # GoogleTest (メイン関数も含む)
)

# (3) CTestにテストを自動登録
include(GoogleTest)
gtest_discover_tests(run_tests)