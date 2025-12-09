// Globals.hpp
#ifndef GLOBALS_HPP
#define GLOBALS_HPP
#pragma once
#include <atomic> // 追加

namespace MyGlobals {
    // グローバル変数や定数をここに定義
    inline std::atomic<int> g_counter = 0; // グローバル変数の例
}



#endif