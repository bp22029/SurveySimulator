#include "../include/portable_random.hpp"

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace portable_random {

namespace {
// M_PI は標準C++ではないので定数で持つ
constexpr double kPi = 3.141592653589793238462643383279502884;
}

uint64_t uniformInt(std::mt19937_64& rng, uint64_t n) {
    if (n == 0) throw std::invalid_argument("uniformInt: n must be positive");
    const uint64_t max = std::numeric_limits<uint64_t>::max();
    const uint64_t limit = max - max % n;
    uint64_t x;
    do {
        x = rng();
    } while (x >= limit);
    return x % n;
}

double uniformReal(std::mt19937_64& rng) {
    return static_cast<double>(rng() >> 11) * 0x1.0p-53;
}

void shuffle(std::vector<int>& v, std::mt19937_64& rng) {
    for (size_t i = v.size(); i > 1; --i) {
        std::swap(v[i - 1], v[uniformInt(rng, i)]);
    }
}

double normal(std::mt19937_64& rng, double mean, double sd) {
    const double u1 = 1.0 - uniformReal(rng); // (0, 1]
    const double u2 = uniformReal(rng);
    return mean + sd * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
}

std::string saveState(const std::mt19937_64& rng) {
    std::ostringstream os;
    os << rng;
    return os.str();
}

void loadState(std::mt19937_64& rng, const std::string& state) {
    std::istringstream is(state);
    is >> rng;
    if (is.fail()) throw std::runtime_error("loadState: invalid mt19937_64 state");
}

} // namespace portable_random
