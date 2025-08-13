cmake_minimum_required(VERSION 3.15)  # バージョンを3.15に修正
project(SurveySimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(SurveySimulator src/main.cpp
        src/data_loader.cpp
        src/prompt_generator.cpp)

target_include_directories(SurveySimulator PUBLIC include)

add_subdirectory(../llama.cpp llama_cpp_build)

target_link_libraries(SurveySimulator PRIVATE llama)