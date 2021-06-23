#include <functional>
#include <condition_variable>
#include <vector>
#include <thread>
#include <future>
#include <utility>
#include "safe_queue.h"

class ThreadPool final {
public:
    ThreadPool(int size) : m_threads(std::vector<std::thread>(size)) {}
    ~ThreadPool() = default;

    void Start()
    {
        for (int i = 0; i < m_threads.size(); ++i) {
            m_threads[i] = std::thread(Worker(this, i));
        }
    }

    void Stop()
    {
        m_isStop = true;
        m_cond.notify_all();    // 唤醒线程池内所有工作的线程
        for (auto& t : m_threads) {
            if (t.joinable()) {
                t.join();   // 通过join函数停掉所有线程
            }
        }
    }

    template<typename Func, typename... Args>
    auto Submit(Func&& f, Args&&... args) -> std::future<decltype(f(args...))>
    {
        std::function<decltype(f(args...))()> task = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);
        auto taskPtr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(task);
        auto worker = [taskPtr]() {
            (*taskPtr)();
        };
        m_queue.Push(worker);
        m_cond.notify_one();
        return taskPtr->get_future();
    }

private:
    class Worker {
    public:
        Worker(ThreadPool* pool, int id) : m_pool(pool), m_workerId(id) {}

        void operator()()
        {
            std::function<void()> task;
            bool isPopSuccess = false;
            while (!m_pool->m_isStop) {
                {
                    std::unique_lock<std::mutex> lck(m_pool->m_mtx);
                    if (m_pool->m_queue.Empty()) {
                        m_pool->m_cond.wait(lck);
                    }
                    isPopSuccess = m_pool->m_queue.Pop(task);
                }
                if (isPopSuccess) {
                    task();
                }
            }
        }
    private:
        ThreadPool* m_pool { nullptr };
        int m_workerId { 0 };
    };

private:
    SafeQueue<std::function<void()>> m_queue;
    std::vector<std::thread> m_threads;
    std::condition_variable m_cond;
    std::mutex m_mtx;
    bool m_isStop { false };
};