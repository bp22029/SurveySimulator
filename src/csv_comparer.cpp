#include "csv_comparer.hpp"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <iostream>

bool isCsvIdentical(const std::string& filePath1, const std::string& filePath2) {
    // バイナリモードでファイルを開く（改行コードの違い等も厳密に検知するため）
    std::ifstream file1(filePath1, std::ios::binary | std::ios::ate);
    std::ifstream file2(filePath2, std::ios::binary | std::ios::ate);

    // ファイルが開けない場合は false を返す（必要に応じてエラー出力等を追加してください）
    if (!file1.is_open() || !file2.is_open()) {
        std::cerr << "Error: Could not open one of the files." << std::endl;
        return false;
    }

    // 1. ファイルサイズを比較
    if (file1.tellg() != file2.tellg()) {
        return false; // サイズが違うなら中身も違う
    }

    // ファイルポインタを先頭に戻す
    file1.seekg(0, std::ios::beg);
    file2.seekg(0, std::ios::beg);

    // 2. 中身をイテレータを使って比較
    // std::equal は指定された範囲の要素が等しいかを判定します
    return std::equal(
        std::istreambuf_iterator<char>(file1.rdbuf()),
        std::istreambuf_iterator<char>(),
        std::istreambuf_iterator<char>(file2.rdbuf())
    );
}

void printCsvComparisonResult(const std::string& filePath1, const std::string& filePath2) {
    if (isCsvIdentical(filePath1, filePath2)) {
        //ファイル名も出しながら結果を表示
        std::cout << "The CSV files are identical: " << filePath1 << " and " << filePath2 << std::endl;
    } else {
        std::cout << "The CSV files differ: " << filePath1 << " and " << filePath2 << std::endl;
    }
}