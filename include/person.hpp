#ifndef PERSON_HPP
#define PERSON_HPP

#include <string>


// 否定的情動性とそのファセット
struct NeuroticismTrait {
    float score;            // 全体のスコア
    float anxiety;          // 不安
    float depression;       // 抑うつ
    float emotional_volatility; // 情緒不安定性
    void updateScore() {
        score = (anxiety + depression + emotional_volatility) / 3.0f;
    }
};

// 勤勉性とそのファセット
struct ConscientiousnessTrait {
    float score;            // 全体のスコア
    float organization;     // 秩序性
    float productivity;     // 生産性
    float responsibility;   // 責任感
    void updateScore() {
        score = (organization + productivity + responsibility) / 3.0f;
    }
};

// 外向性とそのファセット
struct ExtraversionTrait {
    float score;            // 全体のスコア
    float sociability;      // 社交性
    float assertiveness;    // 自己主張性
    float energy_level;     // 活力
    void updateScore() {
        score = (sociability + assertiveness + energy_level) / 3.0f;
    }
};

// 協調性とそのファセット
struct AgreeablenessTrait {
    float score;            // 全体のスコア
    float compassion;       // 思いやり
    float respectfulness;   // 敬意
    float trust;            // 信用
    void updateScore() {
        score = (compassion + respectfulness + trust) / 3.0f;
    }
};

// 開放性とそのファセット
struct OpennessTrait {
    float score;                    // 全体のスコア
    float intellectual_curiosity; // 知的好奇心
    float aesthetic_sensitivity;  // 美的感性
    float creative_imagination;   // 創造的想像力
    void updateScore() {
        score = (intellectual_curiosity + aesthetic_sensitivity + creative_imagination) / 3.0f;
    }
};

// Big Fiveの性格特性を格納する構造体
struct BigFive {
    NeuroticismTrait       neuroticism;
    ConscientiousnessTrait conscientiousness;
    ExtraversionTrait      extraversion;
    AgreeablenessTrait     agreeableness;
    OpennessTrait          openness;
};

struct Schwartz {
    float Self_Direction;
    float Stimulation;
    float Hedonism;
    float Achievement;
    float Power;
    float Security;
    float Conformity;
    float Tradition;
    float Benevolence;
    float Universalism;
};

struct  Schwartz_PVQ {
    float SelfDirection_Thought;
    float SelfDirection_Action;
    float Stimulation;
    float Hedonism;
    float Achievement;
    float Power_Dominance;
    float Power_Resources;
    float Face;
    float Security_Personal;
    float Security_Societal;
    float Tradition;
    float Conformity_Rules;
    float Conformity_Interpersonal;
    float Humility;
    float Benevolence_Caring;
    float Benevolence_Dependability;
    float Universalism_Nature;
    float Universalism_Concern;
    float Universalism_Tolerance;
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
    Schwartz schwartz;
    Schwartz_PVQ schwartz_pvq;
};

#endif // PERSON_HPP