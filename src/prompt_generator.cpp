//
// Created by bp22029 on 2025/08/12.
//
#include "../include/prompt_generator.hpp"
#include <cstdio>
#include <iostream>

std::string readPromptTemplate(const std::string& template_path) {
    // std::ifstream template_file("../data/prompt_template.txt");
    std::ifstream template_file(template_path);
    std::string prompt_template;
    if (template_file.is_open()) {
        std::string line;
        while (std::getline(template_file, line)) {
            prompt_template += line + "\n";
        }
        template_file.close();
        return prompt_template;
    } else {
        return "";
    }
}

void replaceAll(std::string& str, const std::string& placeholder, const std::string& value) {
    size_t pos = 0;
    while ((pos = str.find(placeholder, pos)) != std::string::npos) {
        str.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
}

std::string generatePrompt(const std::string& template_str, const Person& person, const Question& question) {
    std::string prompt = template_str;
    char buffer[32];

    // replaceAll(prompt, "{性別}", person.gender);
    // replaceAll(prompt, "{住所}", person.prefecture_name+person.city_name);
    // replaceAll(prompt, "{年齢}", std::to_string(person.age));
    // replaceAll(prompt, "{職業分類}", person.industry_type);
    // replaceAll(prompt, "{雇用形態}", person.employment_type);
    // replaceAll(prompt,"{企業規模}", person.company_size);
    // replaceAll(prompt, "{世帯構成}", person.family_type);
    // replaceAll(prompt, "{世帯での役割}", person.role_household_type);
    // replaceAll(prompt, "{月収}", std::to_string(person.total_income));

    std::string industry_display = person.industry_type;
    std::string role_display = person.role_household_type;
    std::string income_display = std::to_string(person.total_income);
    std::string family_display = person.family_type; // 世帯構成も微調整できるように変数化

    // --- 2. 「子供」という表記の書き換えロジック（全年齢共通） ---
    // データ上の「子供」を、年齢に合わせて誤解のない日本語に変換する
    if (role_display.find("子供") != std::string::npos) {
        if (person.age >= 18) {
            // 成人の場合
            if (person.gender == "男性") {
                role_display = "実家暮らしの息子（両親と同居）";
            } else if (person.gender == "女性") {
                role_display = "実家暮らしの娘（両親と同居）";
            } else {
                role_display = "実家暮らしの子供（成人・両親と同居）";
            }
        } else {
            // 未成年の場合
            role_display = "親と同居している子供";
        }

        // 補足: 世帯構成の「夫婦と子供」という表記も、48歳などの場合は違和感があるので補足する
        if (person.age >= 30) {
            family_display += "（高齢の親と同居）";
        }
    }

    // --- 3. 学生（24歳以下かつ無職）の判定ロジック ---
    bool is_unemployed = person.industry_type.empty() || person.industry_type == "";
    if (person.age <= 24 && is_unemployed) {
        industry_display = "学生（親の扶養下）";
        if (person.total_income == 0) {
            income_display = "0円（学費・生活費は親負担のため経済的不安なし）";
        }
    }else if (is_unemployed) { // age > 24 は自明なので else if でOK

        // 空白のままだと不安定なので「無職」と明記する
        industry_display = "無職";

        // 0円の理由を補足（これが重要）
        if (person.total_income == 0) {
            // 親と同居しているなら、親の年金等で暮らしている可能性が高い
            if (role_display.find("同居") != std::string::npos) {
                income_display = "0円（家族の収入・年金に依存）";
            } else {
                // 一人暮らしで0円は貯蓄切り崩しか生活保護
                income_display = "0円（貯蓄切り崩し、または支援受給）";
            }
        }
    }

    // --- 4. プロンプトへの置換実行 ---
    replaceAll(prompt, "{性別}", person.gender);
    replaceAll(prompt, "{住所}", person.prefecture_name + person.city_name);
    replaceAll(prompt, "{年齢}", std::to_string(person.age));
    replaceAll(prompt, "{職業分類}", industry_display);
    replaceAll(prompt, "{雇用形態}", person.employment_type);
    replaceAll(prompt, "{企業規模}", person.company_size);

    // ★ここで加工済みの変数を使用
    replaceAll(prompt, "{世帯構成}", family_display);
    replaceAll(prompt, "{世帯での役割}", role_display);
    replaceAll(prompt, "{月収}", income_display);

    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.neuroticism.score);
    //sprintf(buffer, "%.2f", person.personality.neuroticism);
    replaceAll(prompt, "{否定的情動性}", buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.conscientiousness.score);
    //sprintf(buffer, "%.2f", person.personality.conscientiousness);
    replaceAll(prompt, "{勤勉性}", buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.extraversion.score);
    //sprintf(buffer, "%.2f", person.personality.extraversion);
    replaceAll(prompt, "{外向性}", buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.agreeableness.score);
    //sprintf(buffer, "%.2f", person.personality.agreeableness);
    replaceAll(prompt, "{協調性}", buffer);

    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.openness.score);
    //sprintf(buffer, "%.2f", person.personality.openness);
    replaceAll(prompt, "{開放性}", buffer);

    // BFI2 ファセット（否定的情動性）
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.neuroticism.anxiety);
    replaceAll(prompt, "{不安}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.neuroticism.depression);
    replaceAll(prompt, "{抑うつ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.neuroticism.emotional_volatility);
    replaceAll(prompt, "{情緒不安定性}", buffer);

    // BFI2 ファセット（勤勉性）
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.conscientiousness.organization);
    replaceAll(prompt, "{秩序性}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.conscientiousness.productivity);
    replaceAll(prompt, "{生産性}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.conscientiousness.responsibility);
    replaceAll(prompt, "{責任感}", buffer);

    // BFI2 ファセット（外向性）
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.extraversion.sociability);
    replaceAll(prompt, "{社交性}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.extraversion.assertiveness);
    replaceAll(prompt, "{自己主張性}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.extraversion.energy_level);
    replaceAll(prompt, "{活力}", buffer);

    // BFI2 ファセット（協調性）
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.agreeableness.compassion);
    replaceAll(prompt, "{思いやり}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.agreeableness.respectfulness);
    replaceAll(prompt, "{敬意}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.agreeableness.trust);
    replaceAll(prompt, "{信用}", buffer);

    // BFI2 ファセット（開放性）
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.openness.intellectual_curiosity);
    replaceAll(prompt, "{知的好奇心}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.openness.aesthetic_sensitivity);
    replaceAll(prompt, "{美的感性}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.personality.openness.creative_imagination);
    replaceAll(prompt, "{創造的想像力}", buffer);

    // Scwartzの価値観
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Self_Direction);
    replaceAll(prompt, "{自己志向}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Stimulation);
    replaceAll(prompt, "{刺激}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Hedonism);
    replaceAll(prompt, "{快楽}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Achievement);
    replaceAll(prompt, "{達成}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Power);
    replaceAll(prompt, "{権力}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Security);
    replaceAll(prompt, "{安全}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Conformity);
    replaceAll(prompt, "{同調}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Tradition);
    replaceAll(prompt, "{伝統}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Benevolence);
    replaceAll(prompt, "{博愛}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz.Universalism);
    replaceAll(prompt, "{普遍主義}", buffer);

    // Scwartzの価値観（PVQ）
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.SelfDirection_Thought);
    replaceAll(prompt, "{自己志向_思考_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.SelfDirection_Action);
    replaceAll(prompt, "{自己志向_行動_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Stimulation);
    replaceAll(prompt, "{刺激_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Hedonism);
    replaceAll(prompt, "{快楽_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Achievement);
    replaceAll(prompt, "{達成_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Power_Dominance);
    replaceAll(prompt, "{権力_支配_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Power_Resources);
    replaceAll(prompt, "{権力_資源_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Face);
    replaceAll(prompt, "{体面_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Security_Personal);
    replaceAll(prompt, "{安全_個人_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Security_Societal);
    replaceAll(prompt, "{安全_社会_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Tradition);
    replaceAll(prompt, "{伝統_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Conformity_Rules);
    replaceAll(prompt, "{同調_規則_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Conformity_Interpersonal);
    replaceAll(prompt, "{同調_対人_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Humility);
    replaceAll(prompt, "{謙虚_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Benevolence_Caring);
    replaceAll(prompt, "{博愛_配慮_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Benevolence_Dependability);
    replaceAll(prompt, "{博愛_信頼_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Universalism_Nature);
    replaceAll(prompt, "{普遍主義_自然_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Universalism_Concern);
    replaceAll(prompt, "{普遍主義_関心_PVQ}", buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", person.schwartz_pvq.Universalism_Tolerance);
    replaceAll(prompt, "{普遍主義_寛容_PVQ}", buffer);

    replaceAll(prompt, "{質問}", question.text);

    std::string choices_str;
    for (size_t i = 0; i < question.choices.size(); ++i) {
        choices_str += std::to_string(i + 1) + ". " + question.choices[i] + "\n";
    }
    replaceAll(prompt, "{回答選択肢}", choices_str);

    return prompt;
}