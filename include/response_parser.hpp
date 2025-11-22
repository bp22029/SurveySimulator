// response_parser.hpp
#ifndef RESPONSE_PARSER_HPP
#define RESPONSE_PARSER_HPP

#include <string>
#include "nlohmann/json.hpp"

int extractChoiceNumber(const std::string& json_response);

int parseLlmAnswer(const std::string& llmResponse);

int extractAnswerFromTags(const std::string& text);

std::string extractThinkingLog(const std::string& text);

#endif // RESPONSE_PARSER_HPP
