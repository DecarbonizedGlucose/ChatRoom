#pragma once
#include <mutex>
#include <deque>
#include <condition_variable>
#include <vector>
#include <optional>
#include <algorithm>  // for std::remove_if, std::count_if, std::any_of, std::all_of

template<typename T>
class safe_deque : private std::deque<T> {
private:
    mutable std::mutex m_Mutex;
    std::condition_variable m_Condition;
    using base_type = std::deque<T>;

public:
    safe_deque() = default;
    ~safe_deque() = default;

    safe_deque(const safe_deque&) = delete;
    safe_deque& operator=(const safe_deque&) = delete;

    safe_deque(safe_deque&& other) {
        std::lock_guard<std::mutex> lock(other.m_Mutex);
        base_type::operator=(std::move(other));
    }

    safe_deque& operator=(safe_deque&& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(other.m_Mutex);
            base_type::operator=(std::move(other));
        }
        return *this;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return base_type::empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return base_type::size();
    }

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        base_type::push_back(value);
        m_Condition.notify_one();
    }

    void push_back(T&& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        base_type::push_back(std::move(value));
        m_Condition.notify_one();
    }

    void push_front(const T& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        base_type::push_front(value);
        m_Condition.notify_one();
    }

    void push_front(T&& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        base_type::push_front(std::move(value));
        m_Condition.notify_one();
    }

    bool pop_back() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!base_type::empty()) {
            base_type::pop_back();
            return true;
        }
        return false;
    }

    bool pop_back(T& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (base_type::empty()) {
            return false;
        }
        value = std::move(base_type::back());
        base_type::pop_back();
        return true;
    }

    bool pop_front() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!base_type::empty()) {
            base_type::pop_front();
            return true;
        }
        return false;
    }

    bool pop_front(T& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (base_type::empty()) {
            return false;
        }
        value = std::move(base_type::front());
        base_type::pop_front();
        return true;
    }

    void wait_and_pop_back(T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Condition.wait(lock, [this] { return !base_type::empty(); });
        value = std::move(base_type::back());
        base_type::pop_back();
    }

    void wait_and_pop_front(T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Condition.wait(lock, [this] { return !base_type::empty(); });
        value = std::move(base_type::front());
        base_type::pop_front();
    }

    std::optional<T> front() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (base_type::empty()) {
            return std::nullopt;
        }
        return base_type::front();
    }

    std::optional<T> back() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (base_type::empty()) {
            return std::nullopt;
        }
        return base_type::back();
    }

    std::optional<T> at(size_t index) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (index >= base_type::size()) {
            return std::nullopt;
        }
        return base_type::at(index);
    }

    std::optional<T> operator[](size_t index) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (index >= base_type::size()) {
            return std::nullopt;
        }
        return base_type::operator[](index);
    }

    bool get_at(size_t index, T& value) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (index >= base_type::size()) {
            return false;
        }
        value = base_type::at(index);
        return true;
    }

    std::vector<T> copy_all() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return std::vector<T>(base_type::begin(), base_type::end());
    }

    // 获取最后的max_count个元素的副本
    std::vector<T> copy_max_num(size_t max_count) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (max_count >= base_type::size()) {
            return std::vector<T>(base_type::begin(), base_type::end());
        }
        return std::vector<T>(base_type::end() - max_count, base_type::end());
    }

    template<typename Func>
    void for_each(Func&& func) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto& item : static_cast<const base_type&>(*this)) {
            func(item);
        }
    }

    template<typename Func>
    void for_each_mutable(Func&& func) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto& item : static_cast<base_type&>(*this)) {
            func(item);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        base_type::clear();
    }

    bool insert(size_t index, const T& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (index > base_type::size()) {
            return false;
        }
        base_type::insert(base_type::begin() + index, value);
        m_Condition.notify_one();
        return true;
    }

    bool erase(size_t index) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (index >= base_type::size()) {
            return false;
        }
        base_type::erase(base_type::begin() + index);
        return true;
    }
};