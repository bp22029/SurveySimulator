#ifndef PERSON_HPP
#define PERSON_HPP

#include <string>

// Big Fiveの性格特性を格納する構造体
struct BigFive {
    float neuroticism;       // 神経症傾向
    float conscientiousness; // 誠実性
    float extraversion;      // 外向性
    float agreeableness;     // 協調性
    float openness;          // 開放性
};

// 一人の人間を表す構造体
struct Person {
    int person_id;
    std::string gender;
    std::string prefecture_name;
    int age;
    std::string industry_type;
    std::string family_type;
    std::string role_household_type;
    std::string employment_type;
    std::string company_size;
    int total_income;
    BigFive personality;
};

#endif // PERSON_HPP