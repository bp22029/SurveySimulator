// individual_responses.hpp
#ifndef INDIVIDUAL_RESPONSES_HPP
#define INDIVIDUAL_RESPONSES_HPP

#include <map>
#include <string>

struct IndividualResponse {
    int person_id;
    std::map<std::string, int> responses; // question_id -> choice_number
};

#endif // INDIVIDUAL_RESPONSES_HPP
