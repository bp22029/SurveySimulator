// 最適化のチェックポイントから、今後の実験に使う形の CSV（合成人口の元の列 ＋ 最適化した個性）を書き出す
//
// 使い方: ExportPersonality <config.json> [output.csv]
//   output.csv を省略すると run_dir に synthetic_population_with_bfi2_<モデル名>_<client_id>_sweepNNN.csv で書く
//   （NNN はチェックポイントで完了している最後の周）。最適化の途中でも使える。サーバーには接続しない

#include <exception>
#include <iostream>
#include <string>
#include "../include/batch_optimizer.hpp"

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: " << argv[0] << " <config.json> [output.csv]" << std::endl;
        return 2;
    }
    try {
        const BatchOptimizerConfig config = loadBatchOptimizerConfig(argv[1]);
        std::cout << "Wrote " << exportPersonalityFromCheckpoint(config, argc == 3 ? argv[2] : "") << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
