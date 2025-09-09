// individual_response_manager.hpp
#ifndef INDIVIDUAL_RESPONSE_MANAGER_HPP
#define INDIVIDUAL_RESPONSE_MANAGER_HPP

#include <vector>
#include <map>
#include <string>
#include "individual_responses.hpp"

class IndividualResponseManager {
private:
    std::map<int, IndividualResponse> person_responses; // person_id -> IndividualResponse

public:
    void recordResponse(int person_id, const std::string& question_id, int choice_number);
    IndividualResponse getPersonResponses(int person_id) const;
    void exportToCSV(const std::string& filename, const std::vector<std::string>& question_ids) const;
    void printSummary() const;
};

#endif // INDIVIDUAL_RESPONSE_MANAGER_HPP


// IndividualResponseManager
// └── person_responses (std::map)
//     ├── [0] → IndividualResponse
//     │   ├── person_id: 0
//     │   └── responses: std::map<string, int>
//     │       ├── "dq2_1" → 2
//     │       ├── "dq2_2" → 4
//     │       ├── "dq2_3" → 1
//     │
//     ├── [1] → IndividualResponse
//     │   ├── person_id: 1
//     │   └── responses: std::map<string, int>
//     │       ├── "dq2_1" → 3
//     │       ├── "dq2_2" → 1
//     │       ├── "dq2_3" → 1
//     │
//     ├── [2] → ...
//
