#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <vector>
#include "safe_queue.hpp"

class thread_pool {
private:
    /* ------------------------------------------- */
    class thread_worker {
    private:
        thread_pool* pool_ptr;
        int m_Id;
        std::thread m_Thread;

    public:
        bool isWorking;
        bool terminate;

        thread_worker(thread_pool* pool, int id) : pool_ptr(pool), m_Id(id) {}

        void operator()() {
            while (pool_ptr->pool_status <= 1) {
                std::function<void()> func;
                {
                    std::unique_lock<std::mutex> lock(pool_ptr->m_Mutex);
                    pool_ptr->m_Condition.wait(lock, [&]{
                        return pool_ptr->pool_status == 1 || !pool_ptr->m_TaskQueue.empty();
                    });
                    if (pool_ptr->pool_status == 1 && pool_ptr->m_TaskQueue.empty())
                        return;
                    if (!pool_ptr->m_TaskQueue.pop(func))
                        continue;
                }
                func();
            }
        }
    };
    /* ------------------------------------------- */
    safe_queue<std::function<void()>> m_TaskQueue;
    std::vector<std::thread> m_Workers;
public:
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    int pool_core_size;
    //int pool_max_size;
    int pool_status = 0; // 0: working, 1: shutdown, 2: stop, 3: tidying, 4: terminated
    bool is_shutting_down = false;

    thread_pool(int core_size = 4/*, int max_size = 8*/ )
        : pool_core_size(core_size) {}

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    thread_pool& operator=(thread_pool&&) = delete;

    ~thread_pool() {
        stop();
        tidy();
    }

    void init() {
        pool_status = 0;
        for (int i = 0; i < pool_core_size; ++i) {
            m_Workers.push_back(std::thread(thread_worker(this, i)));
        }
    }

    void shutdown() {
        pool_status = 1;
        m_Condition.notify_all();
        for (auto& worker : m_Workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        tidy();
    }

    void stop() {
        pool_status = 2;
        m_Condition.notify_all(); // 必须唤醒所有worker
        for (auto& worker : m_Workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        tidy();
    }

    void tidy() {
        pool_status = 3;
        m_Workers.clear();
        m_TaskQueue.clear();
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
        using return_type = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        std::function<return_type()> task_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(task_func);
        std::function<void()> l_func = [task_ptr]() {
            (*task_ptr)();
        };
        if (pool_status != 0) {
            return std::future<return_type>();
        }
        m_TaskQueue.push(l_func);
        m_Condition.notify_one();
        return task_ptr->get_future();
    }

    void submit(std::function<void()> func) {
        if (pool_status != 0) {
            return;
        }
        m_TaskQueue.push(std::move(func));
        m_Condition.notify_one();
    }
};
