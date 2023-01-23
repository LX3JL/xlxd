#pragma once

#include <condition_variable>
#include <mutex>

class CSimpleCondition final
{
public:
    CSimpleCondition() : m_Mutex(), m_Condition() {}
    CSimpleCondition(const CSimpleCondition&) = delete;
    CSimpleCondition& operator=(const CSimpleCondition&) = delete;
    CSimpleCondition(CSimpleCondition&&) = delete;
    ~CSimpleCondition() {};

    // Wait up to @duration to be signaled, or until @predicate is true.
    // Returns result of predicate after timing out or being signaled.
    template<typename Duration, typename Predicate>
    bool wait(Duration, Predicate);

    // Signal waiters. If @all is true, all waiters will be woken up.
    void signal(bool all=true)
    {
        if (all)
            m_Condition.notify_all();
        else
            m_Condition.notify_one();
    }

private:
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
};

// Note: @timeout is a relative duration, e.g., "30s".
template<typename Duration, typename Predicate>
bool CSimpleCondition::wait(Duration timeout, Predicate predicate)
{
    std::unique_lock<std::mutex> lock(m_Mutex);
    auto bound = std::chrono::system_clock::now() + timeout;
    return m_Condition.wait_until(lock, bound, predicate); 
}
