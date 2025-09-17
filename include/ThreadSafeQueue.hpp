//
// Created by bp22029 on 2025/09/13.
//

#ifndef SURVEYSIMULATOR_THREADSAFEQUEUE_H
#define SURVEYSIMULATOR_THREADSAFEQUEUE_H


#include <vector>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "person.hpp"
#include "question.hpp"

// 1つの処理単位を表す構造体
struct SurveyTask {
    Person person;
    Question question;
};

// 1つの処理結果を表す構造体
struct TaskResult {
    int person_id;
    std::string question_id;
    int choice_number;
};

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> data_queue; // 1. データを格納する本体
    mutable std::mutex mtx; // 2. キューへのアクセスを制御するミューテックス(相互排他の鍵)
    std::condition_variable cv; // 3. スレッドへの通知役（呼び鈴）
    bool finished = false; // 4. キューが終了したかどうかのフラグ
public:
    // データの追加
    void push(T data) {
        std::lock_guard<std::mutex> lock(mtx); // 1. ミューテックスで保護
        data_queue.push(std::move(data)); // 2. データをキューに追加
        cv.notify_one(); // 3. 待機中のスレッドに通知
    }

    // データの取得（待機あり）
    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx); // 1. ミューテックスで保護(特殊なロック)
        // 2. キューが空でなくなるか、終了フラグが立つまで待機
        cv.wait(lock, [this]{ return !data_queue.empty() || finished; });
        // 3. キューが空で終了している場合はfalseを返す(終了判定)
        if (data_queue.empty() && finished) {
            return false;
        }
        // 4. キューからデータを取り出す
        value = std::move(data_queue.front());// 5. 取り出したデータをキューから削除
        data_queue.pop();
        return true;
    }

    void notify_finished() {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
        cv.notify_all();
    }
};


#endif //SURVEYSIMULATOR_THREADSAFEQUEUE_H