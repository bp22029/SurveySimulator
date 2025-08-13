#ifndef QUESTION_HPP
#define QUESTION_HPP

#include <string>
#include <vector>

struct Question {
    std::string id;
    std::string text;
    std::vector<std::string> choices;
};

#endif // QUESTION_HPP