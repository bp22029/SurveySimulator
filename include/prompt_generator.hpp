#ifndef PROMPT_GENERATOR_HPP
#define PROMPT_GENERATOR_HPP

#include <string>
#include "person.hpp"
#include "question.hpp"

// プロンプト生成関数
std::string generatePrompt(const std::string& template_str, const Person& person, const Question& question);

// 文字列置換ユーティリティ
void replaceAll(std::string& str, const std::string& placeholder, const std::string& value);

#endif // PROMPT_GENERATOR_HPP