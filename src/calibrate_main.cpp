// SA の温度を決めるための ΔE の測定（job_server を使う）
//
// 使い方: CalibrateTemperature <config.json> <rounds> <out_dir>
//   config.json  BatchOptimizer の本番と同じ設定ファイル（推論条件・seed・プロンプト・変異の幅を揃えるため）
//   rounds       初期状態から変更案を作って ΔE を測る回数（1回 = 全員分の推論1ジョブ）
//   out_dir      結果の出力先（本番の run_dir とは別にする）
// 途中で止まっても、同じ引数で起動し直せば、終わったジョブはサーバーの結果をそのまま使う。

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include "../include/batch_optimizer.hpp"
#include "../include/calibrate_temperature.hpp"
#include "../include/job_client.hpp"

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " <config.json> <rounds> <out_dir>" << std::endl;
        return 2;
    }
    try {
        const BatchOptimizerConfig config = loadBatchOptimizerConfig(argv[1]);
        const int rounds = std::stoi(argv[2]);
        std::cout << "job_server: " << config.server_url << " (from " << config.server_url_source << ")" << std::endl;
        JobClient client(config.server_url, std::make_shared<CurlTransport>());
        runCalibration(config, rounds, argv[3], client);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
