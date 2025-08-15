cmake_minimum_required(VERSION 3.15)  # バージョンを3.15に修正
project(SurveySimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



# 必要なライブラリをFetchContentで取得
include(FetchContent)

# 1. cpp-httplib (HTTP Client)
FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3 # 安定したバージョンを指定
)
FetchContent_MakeAvailable(httplib)

# 2. nlohmann/json (JSON Parser)
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3 # 安定したバージョンを指定
)
FetchContent_MakeAvailable(json)

add_executable(SurveySimulator src/main.cpp
        src/data_loader.cpp
        src/prompt_generator.cpp
)

target_include_directories(SurveySimulator PRIVATE
        ${httplib_SOURCE_DIR}
        ${json_SOURCE_DIR}/include
)

target_include_directories(SurveySimulator PUBLIC include)

add_subdirectory(../llama.cpp llama_cpp_build)

target_link_libraries(SurveySimulator PRIVATE llama)