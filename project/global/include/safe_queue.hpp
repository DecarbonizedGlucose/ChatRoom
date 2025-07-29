#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

template<typename T>
class safe_queue {
private:
    std::mutex m_Mutex;
    std::queue<T> m_Queue;
    std::condition_variable m_Condition;
public:
    safe_queue() = default;
    ~safe_queue() = default;
    safe_queue(const safe_queue&) = delete;
    safe_queue& operator=(const safe_queue&) = delete;

    bool empty() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        return m_Queue.empty();
    }

    int size() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        return m_Queue.size();
    }

    void push(const T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Queue.push(value);
        m_Condition.notify_one(); // 通知等待的线程
    }

    void push(T&& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Queue.push(std::move(value));
        m_Condition.notify_one(); // 通知等待的线程
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        if (m_Queue.empty()) {
            return false;
        }
        value = std::move(m_Queue.front());
        m_Queue.pop();
        return true;
    }

    // 阻塞等待直到队列非空，然后取出一个元素
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Condition.wait(lock, [this] { return !m_Queue.empty(); });
        value = std::move(m_Queue.front());
        m_Queue.pop();
    }

    // 带超时的阻塞等待
    template<typename Rep, typename Period>
    bool wait_for_and_pop(T& value, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        if (m_Condition.wait_for(lock, timeout, [this] { return !m_Queue.empty(); })) {
            value = std::move(m_Queue.front());
            m_Queue.pop();
            return true;
        }
        return false; // 超时
    }

    void clear() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (!m_Queue.empty()) {
            m_Queue.pop();
        }
    }
};
