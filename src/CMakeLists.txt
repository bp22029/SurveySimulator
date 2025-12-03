# (1) ロジック部分をライブラリ "SurveySimulatorLib" として定義
# main.cpp と ThreadSafeQueue.hpp (ヘッダオンリー) 以外をリストアップ
add_library(SurveySimulatorLib STATIC
        data_loader.cpp
        prompt_generator.cpp
        llm_client.cpp
        survey_results.cpp
        simulation_runner.cpp
        response_parser.cpp
        individual_response_manager.cpp
        thread_worker.cpp
        cross_table_generator.cpp
        llm_offline.cpp
        csv_comparer.cpp

        # ヘッダファイルも追加 (includeディレクトリへのパスは (3) で指定)
        ../include/data_loader.hpp
        ../include/prompt_generator.hpp
        ../include/llm_client.hpp
        ../include/survey_results.hpp
        ../include/simulation_runner.hpp
        ../include/response_parser.hpp
        ../include/individual_response_manager.hpp
        ../include/thread_worker.hpp
        ../include/cross_table_generator.hpp
        ../include/person.hpp
        ../include/question.hpp
        ../include/individual_responses.hpp
        ../include/ThreadSafeQueue.hpp
        ../include/llm_offline.hpp
        ../include/csv_comparer.hpp
        ../include/globals.hpp
)

# (2) メインの実行ファイル "SurveySimulator" を定義
add_executable(SurveySimulator
        main.cpp
)

# (3) 各ターゲットに必要なインクルードパスとリンクを指定

# SurveySimulatorLib が必要とするライブラリ
target_include_directories(SurveySimulatorLib PUBLIC
        ../include                      # 自分のヘッダ
        ${httplib_SOURCE_DIR}           # httplibのヘッダ
        ${json_SOURCE_DIR}/include      # jsonのヘッダ
)

# SurveySimulator（実行ファイル） が必要とするライブラリ
target_link_libraries(SurveySimulator PRIVATE
        SurveySimulatorLib              # (1)で定義した自作ライブラリ
)