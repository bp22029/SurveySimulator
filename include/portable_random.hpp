#ifndef PORTABLE_RANDOM_HPP
#define PORTABLE_RANDOM_HPP

// 処理系に依存しない乱数（設計書 §4.3）
// std::*_distribution と std::shuffle は処理系によって結果が変わるので使わない。
// エンジン std::mt19937_64 の出力列は規格で決まっているので、これだけを使う。

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace portable_random {

// 0 〜 n-1 の一様な整数（棄却法）
uint64_t uniformInt(std::mt19937_64& rng, uint64_t n);

// [0, 1) の一様な実数（53bit）
double uniformReal(std::mt19937_64& rng);

// Fisher-Yates シャッフル
void shuffle(std::vector<int>& v, std::mt19937_64& rng);

// 正規分布（Box-Muller。1回の呼び出しで一様乱数を必ず2個消費する）
double normal(std::mt19937_64& rng, double mean, double sd);

// エンジンの状態の保存と復元（テキスト表現は規格で決まっている）
std::string saveState(const std::mt19937_64& rng);
void loadState(std::mt19937_64& rng, const std::string& state);

} // namespace portable_random

#endif // PORTABLE_RANDOM_HPP
