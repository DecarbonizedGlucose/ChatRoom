#ifndef SAFE_QUEUE_HPP
#define SAFE_QUEUE_HPP

#include <mutex>
#include <queue>

template<typename T>
class safe_queue {
private:
    std::mutex m_Mutex;
    std::queue<T> m_Queue;
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
    }

    void push(T&& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Queue.push(std::move(value));
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

    void clear() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (!m_Queue.empty()) {
            m_Queue.pop();
        }
    }
};

#endif // SAFE_QUEUE_HPP