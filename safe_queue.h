#include <queue>
#include <mutex>

template<typename T>
class SafeQueue final {
public:
    void Push(const T& t)
    {
        std::unique_lock<std::mutex> lck(m_mtx);
        m_task.push(t);
    }

    bool Pop(T& t)
    {
        std::unique_lock<std::mutex> lck(m_mtx);
        if (m_task.empty()) {
            return false;
        }
        t = std::move(m_task.front());
        m_task.pop();
        return true;
    }

    bool Empty()
    {
        std::unique_lock<std::mutex> lck(m_mtx);
        return m_task.empty();
    }

    int Size()
    {
        std::unique_lock<std::mutex> lck(m_mtx);
        return m_task.size();
    }
private:
    std::queue<T> m_task;
    std::mutex m_mtx;
};