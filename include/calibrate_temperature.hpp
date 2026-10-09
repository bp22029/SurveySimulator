#ifndef CALIBRATE_TEMPERATURE_HPP
#define CALIBRATE_TEMPERATURE_HPP

// SA の初期温度・終了温度を決めるための ΔE（ΔTAE）の測定。
//
// 昨年度は山田らの方法に倣い、悪化した変更案を受け入れる割合（μAG比）が 50％ になる温度を初期温度、
// 0.2％ になる温度を終了温度とし、悪化した ΔE の平均 ΔĒ から T = ΔĒ / ln(1/p) で決めた
// （Qwen3-14B で ΔĒ ≈ 0.0141 → 0.02034 / 0.002269）。モデルが変わると ΔE の大きさも変わるので測り直す。
//
// 初期状態（BatchOptimizer の sweep 0 と同じ個性・回答）から、全員に変更案を1つずつ作って推論し、
// 「その人の回答だけを入れ替えたときの ΔTAE」を1人ずつ独立に求める（採否は判定しない）。
// これを rounds 回繰り返す。推論条件・乱数・プロンプトは BatchOptimizer と同じ関数で作る。
//
// sweep 0 と round 1 は BatchOptimizer の sweep 0・sweep 1 とまったく同じ入力になるので、本番と同じ
// client_id で投げる（本番を始めたとき、サーバーは同じジョブの結果を返し、推論し直さない）。
// round 2 以降は "<client_id>-calib" で投げる。

#include <functional>
#include <string>
#include <vector>
#include "batch_optimizer.hpp"
#include "job_client.hpp"

// この差より小さい ΔTAE は 0（変化なし）とみなす。TAE を足し上げる順番の違いで出る丸め誤差を除くため
constexpr double kDeltaTolerance = 1e-12;

struct DeltaStats {
    int n_proposals = 0;   // 判定できた変更案（回答が1件も解析できなかった人を除く）
    int n_skipped = 0;     // 回答が1件も解析できなかった人
    int n_worse = 0;       // ΔE > 0
    int n_better = 0;      // ΔE < 0
    int n_zero = 0;        // ΔE = 0（回答のビンが変わらなかった、または打ち消し合った）
    double mean_worse = 0.0;
};

DeltaStats summarizeDeltas(const std::vector<double>& deltas, int n_skipped);

// ΔE > 0 のものだけを取り出す
std::vector<double> worseDeltas(const std::vector<double>& deltas);

// 昨年度の方法：悪化の平均 ΔĒ を確率 p で受け入れる温度 T = ΔĒ / ln(1/p)
double temperatureFromMeanDelta(double mean_worse, double acceptance);

// 温度 T で、悪化した変更案を受け入れる割合の期待値（mean of exp(-ΔE/T)）
double expectedAcceptance(const std::vector<double>& worse, double temperature);

// 分布に基づく方法：expectedAcceptance がちょうど p になる温度（二分法）
double temperatureForAcceptance(const std::vector<double>& worse, double acceptance);

struct CalibrationRound {
    int round = 0;
    std::string client_id;
    int sweep = 0;
    int job_id = 0;
    int n_unparsed = 0;
    int n_length = 0;
    double elapsed_sec = -1.0;     // サーバーでの推論時間（開始から終了まで。キューで待った時間は含まない）。不明なら負
    std::vector<int> person_ids;   // population の並び
    std::vector<double> deltas;    // person_ids と同じ並び。skipped の人は 0
    std::vector<bool> skipped;
};

struct CalibrationResult {
    double initial_tae = 0.0;
    int initial_job_id = 0;
    double initial_elapsed_sec = -1.0;
    std::vector<CalibrationRound> rounds;
};

// 測定して out_dir に結果を書く。out_dir は本番の run_dir と別にすること
CalibrationResult runCalibration(const BatchOptimizerConfig& config, int rounds, const std::string& out_dir,
                                 JobClient& client, const std::function<void(int)>& sleep_sec = nullptr);

#endif // CALIBRATE_TEMPERATURE_HPP
