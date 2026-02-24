# (1) ロジック部分をライブラリ "SurveySimulatorLib" として定義
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
        optimization_manager.cpp
        experiment_runner.cpp
        experiment_runner_parallel.cpp

        # ヘッダファイル
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
        ../include/optimization_manager.hpp
        ../include/experiment_runner.hpp
        ../include/experiment_runner_parallel.hpp
)

# (2) メインの実行ファイル "SurveySimulator" を定義
add_executable(SurveySimulator
        main.cpp
)

# (3) CURLパッケージの検索
find_package(CURL REQUIRED)

# (4) インクルードディレクトリの設定
target_include_directories(SurveySimulatorLib PUBLIC
        ../include
        ${httplib_SOURCE_DIR}
        ${json_SOURCE_DIR}/include
        ${CURL_INCLUDE_DIRS}
)

# (5) ★ここが重要: SurveySimulatorLib に CURL をリンクする
target_link_libraries(SurveySimulatorLib PUBLIC
        ${CURL_LIBRARIES}
)

# (6) 実行ファイルは自作ライブラリをリンクするだけでOK
target_link_libraries(SurveySimulator PRIVATE
        SurveySimulatorLib
)