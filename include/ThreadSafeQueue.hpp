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

// 以前提示したスレッドセーフなキュー（再掲）
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> data_queue;
    mutable std::mutex mtx;
    std::condition_variable cv;
    bool finished = false;
public:
    void push(T data) {
        std::lock_guard<std::mutex> lock(mtx);
        data_queue.push(std::move(data));
        cv.notify_one();
    }

    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return !data_queue.empty() || finished; });
        if (data_queue.empty() && finished) {
            return false;
        }
        value = std::move(data_queue.front());
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