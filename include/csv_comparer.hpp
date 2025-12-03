#ifndef CSV_COMPARER_HPP
#define CSV_COMPARER_HPP

#include <string>

/**
 * 2つのCSVファイルが完全に一致するかを確認する関数
 * @param filePath1 比較するファイル1のパス
 * @param filePath2 比較するファイル2のパス
 * @return 一致していれば true, 異なれば false
 */
bool isCsvIdentical(const std::string& filePath1, const std::string& filePath2);

void printCsvComparisonResult(const std::string& filePath1, const std::string& filePath2);

#endif // CSV_COMPARER_HPP