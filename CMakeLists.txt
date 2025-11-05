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

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1 # (適宜バージョン確認)
)

set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(googletest)

# CTest (CMakeのテスト機能) を有効化
enable_testing()

# カバレッジ設定 (Debugビルド時のみ有効)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Coverage flags enabled for Debug build")

    # ▼▼▼ 以下のように修正 ▼▼▼
    if(MSVC)
        # MSVC (Visual Studio) の場合は /PROFILE を使用
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /PROFILE")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /PROFILE")
    else()
        # GCC/Clang の場合は --coverage を使用
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()

# サブディレクトリを追加
add_subdirectory(src)
add_subdirectory(tests)


