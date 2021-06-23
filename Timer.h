#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

class Timer final {
public:
    Timer() = default;
    ~Timer()
    {
        Stop();
    }

    template<typename Func, typename... Args>
    void Start(int time, Func&& f, Args&&... args)
    {
        if (!m_expired.load()) {
            return;
        }
        m_expired.store(false);
        std::function<decltype(f(args...))()> task = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);
        std::thread([this, time, task]() {
            while (!m_tryToExpired.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(time));
                task();
            }

            {
                std::unique_lock<std::mutex> m_lck(m_mtx);
                m_expired.store(true);
                m_cond.notify_one();
            }
        }).detach();
    }

    void Stop()
    {
        if (m_expired.load()) {
            return;
        }
        if (m_tryToExpired.load()) {
            return;
        }
        m_tryToExpired.store(true);
        {
            std::unique_lock<std::mutex> m_lck(m_mtx);
            m_cond.wait(m_lck, [this](){
                return m_expired.load();
            });
            if (m_expired.load()) {
                m_tryToExpired.store(false);
            }
        }
    }

private:
    std::atomic<bool> m_expired { true };
    std::atomic<bool> m_tryToExpired { false };
    std::mutex m_mtx;
    std::condition_variable m_cond;
};