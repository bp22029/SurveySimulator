// 一括方式の SA による個性特性の最適化（job_server を使う）
//
// 使い方: BatchOptimizer <config.json>
// 設定ファイルの例: config/batch_optimizer.example.json
// 途中で止まっても、同じ設定ファイルで起動し直せばチェックポイントから再開する。

#include <exception>
#include <iostream>
#include <memory>
#include "../include/batch_optimizer.hpp"
#include "../include/job_client.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <config.json>" << std::endl;
        return 2;
    }
    try {
        const BatchOptimizerConfig config = loadBatchOptimizerConfig(argv[1]);
        JobClient client(config.server_url, std::make_shared<CurlTransport>());
        runBatchOptimization(config, client);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
