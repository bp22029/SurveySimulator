#ifndef PERSON_HPP
#define PERSON_HPP

#include <string>


// 否定的情動性とそのファセット
struct NeuroticismTrait {
    float score;            // 全体のスコア
    float anxiety;          // 不安
    float depression;       // 抑うつ
    float emotional_volatility; // 情緒不安定性
};

// 勤勉性とそのファセット
struct ConscientiousnessTrait {
    float score;            // 全体のスコア
    float organization;     // 秩序性
    float productivity;     // 生産性
    float responsibility;   // 責任感
};

// 外向性とそのファセット
struct ExtraversionTrait {
    float score;            // 全体のスコア
    float sociability;      // 社交性
    float assertiveness;    // 自己主張性
    float energy_level;     // 活力
};

// 協調性とそのファセット
struct AgreeablenessTrait {
    float score;            // 全体のスコア
    float compassion;       // 思いやり
    float respectfulness;   // 敬意
    float trust;            // 信用
};

// 開放性とそのファセット
struct OpennessTrait {
    float score;                    // 全体のスコア
    float intellectual_curiosity; // 知的好奇心
    float aesthetic_sensitivity;  // 美的感性
    float creative_imagination;   // 創造的想像力
};

// Big Fiveの性格特性を格納する構造体
struct BigFive {
    NeuroticismTrait       neuroticism;
    ConscientiousnessTrait conscientiousness;
    ExtraversionTrait      extraversion;
    AgreeablenessTrait     agreeableness;
    OpennessTrait          openness;
};


// // Big Fiveの性格特性を格納する構造体
// struct BigFive {
//     float neuroticism;       // 否定的情動性
//     float conscientiousness; // 勤勉性
//     float extraversion;      // 外向性
//     float agreeableness;     // 協調性
//     float openness;          // 開放性
// };

// 一人の人間を表す構造体
struct Person {
    int person_id;
    std::string gender;
    std::string prefecture_name;
    std::string city_name;
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