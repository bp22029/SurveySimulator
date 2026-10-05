#ifndef TAE_TRACKER_HPP
#define TAE_TRACKER_HPP

// TAE（回答分布の総絶対誤差）の計算。OptimizationManager と同じ定義・同じビン分け。
//
// OptimizationManager は TAE を差分の足し引きで更新するため、浮動小数点の誤差が蓄積し、
// チェックポイントから再開すると通しで回した場合と最後の桁がずれうる。
// こちらは TAE を常に「回答の件数（整数）から、決まった質問順で足し上げた値」として計算するので、
// 同じ回答の集合からは必ず同じ TAE になる。

#include <map>
#include <string>
#include <vector>

class TaeTracker {
public:
    using Answers = std::map<std::string, int>; // question_id -> 選択肢番号

    // real_ratios.csv（question_id, ビンごとの比率...）を読み込む
    bool loadRealData(const std::string& filename);
    void setRealData(const std::map<std::string, std::vector<double>>& real_ratios);

    // 全員の回答から件数を数え直す。question_ids の順番が足し上げの順番になる
    void initialize(const std::map<int, Answers>& all_answers, int population_size,
                    const std::vector<std::string>& question_ids);

    double total() const { return total_; }

    // 1人の回答を old から new に置き換えた場合の TAE（状態は変えない）
    // new_answers に無い質問は old のまま（解析できなかった回答は置き換えない）
    double evaluate(const Answers& old_answers, const Answers& new_answers) const;

    // 置き換えを確定する
    void commit(const Answers& old_answers, const Answers& new_answers);

    static int mapChoiceToBin(const std::string& question_id, int choice);

private:
    struct Change {
        size_t question_index;
        int old_bin;
        int new_bin;
    };
    std::vector<Change> changes(const Answers& old_answers, const Answers& new_answers) const;
    double questionError(size_t qi, const std::vector<int>& counts) const;
    double sumErrors(const std::vector<double>& errors) const;

    std::map<std::string, std::vector<double>> real_ratios_;
    std::vector<std::string> question_ids_;            // 正解のある質問だけ、足し上げの順
    std::map<std::string, size_t> question_index_;
    std::vector<std::vector<double>> targets_;
    std::vector<std::vector<int>> counts_;
    std::vector<double> errors_;
    int population_size_ = 0;
    double total_ = 0.0;
};

#endif // TAE_TRACKER_HPP
