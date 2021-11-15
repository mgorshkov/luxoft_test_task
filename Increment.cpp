//
//  Increment.cpp
//  luxoft_test_task
// 1.Use two threads to increment an integer. Thread A increments when even and Thread B increments when odd (for the integer problem we can have it specify up to a number provided on the command line)
// 1a. What are some of the difficulties in adding more threads? Please show the difficulties with code.
// 1b. Extra credit – Design an improved solution to the above that can scale with many threads

// I have considered 3 different solutions:
// * Double-check with std::mutex
// * Double-check with lock-free mutex
// * Wait-notify approach with std::condition_variable
// Out of these 3 approaches, we get the best performance and scaling with wait-notify.
// Some problems with mutexes approach:
// 1) performance significantly drops as number of threads increases
// 2) CPU consumption increases with number of threads
// Reasons:
// 1) threads waste time on waiting since it adds some overhead
// 2) the scheduler won't give them time quants when waiting thus not allowing to work when wait is close to its completion

// Parameters:
// num_threads=50, max_value=1000
// Results:
// std::mutex
// main: 13414489 micros passed
// main: 12973986 micros passed
// SpinLockMutex
// main: 12635709 micros passed
// main: 12003058 micros passed
// condition_variable
// main: 39448 micros passed
// main: 38970 micros passed

//  Created by Mikhail Gorshkov on 21.08.2021.
//

#include <list>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <atomic>

//#define DEBUG_LOG

//#define STD_MUTEX
//#define SPINLOCK_MUTEX
#define WAIT_NOTIFY

#include "TimeLogger.hpp"

class SpinLockMutex {
public:
    void lock() {
        while (m_flag.test_and_set());
    }
    void unlock() {
        m_flag.clear();
    }
private:
    std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
};

template<typename Mutex>
class Incrementers;

template<typename Mutex>
class Incrementer {
public:
    Incrementer(Incrementers<Mutex>* parent, int total_threads, int num_thread, int max_value);
    
    explicit Incrementer(const Incrementer&) = delete;
    explicit Incrementer(Incrementer&&) = delete;
    
    ~Incrementer();
    
    bool inc();
    
private:
    bool is_our_value() const;
    
    void thread_proc();

    Incrementers<Mutex>* m_parent;
    int m_total_threads;
    int m_num_thread;
    int m_max_value;
    
    std::thread m_thread;
};

template<typename Mutex>
class Incrementers {
public:
    Incrementers(int total_threads, int max_value)
    {
        for (int i = 0; i <total_threads; ++i) {
            m_incrementers.emplace_back(this, total_threads, i, max_value);
        }
    }
    
    explicit Incrementers(const Incrementers&) = delete;
    explicit Incrementers(Incrementers&&) = delete;
    
private:
    std::atomic_bool m_finish{false};
    std::atomic_int m_value{0};
    
    Mutex m_mutex;
#ifdef WAIT_NOTIFY
    std::condition_variable m_cv;
#endif

    std::list<Incrementer<Mutex>> m_incrementers;
    
    friend class Incrementer<Mutex>;
};

template<typename Mutex>
Incrementer<Mutex>::Incrementer(Incrementers<Mutex>* parent, int total_threads, int num_thread, int max_value)
    : m_thread(&Incrementer::thread_proc, this)
    , m_parent{parent}
    , m_total_threads{total_threads}
    , m_num_thread{num_thread}
    , m_max_value{max_value}
{
}
    
template<typename Mutex>
Incrementer<Mutex>::~Incrementer() {
    m_thread.join();
}
    
template<typename Mutex>
bool Incrementer<Mutex>::inc() {
    if (m_parent->m_value > m_max_value) {
#ifdef DEBUG_LOG
        std::cout << "m_value exceeded, returning false: " << m_parent->m_value << ", i=" << m_num_thread << std::endl;
#endif
        return false;
    }
#ifdef DEBUG_LOG
    std::cout << "m_value=" << m_parent->m_value << ", m_num_thread=" << m_num_thread << std::endl;
#endif
    ++m_parent->m_value;
    return true;
}
    
template<typename Mutex>
bool Incrementer<Mutex>::is_our_value() const {
    return m_parent->m_value % m_total_threads == m_num_thread;
}

template<typename Mutex>
void Incrementer<Mutex>::thread_proc() {
    while (!m_parent->m_finish) {
#if defined(STD_MUTEX) || defined(SPINLOCK_MUTEX)
        if (!is_our_value()) {
            continue;
        }
        std::lock_guard<Mutex> lk(m_parent->m_mutex);
        if (!is_our_value()) {
            continue;
        }
        if (!inc()) {
            m_parent->m_finish = true;
        }
#elif defined(WAIT_NOTIFY)
        {
            std::unique_lock<std::mutex> lk(m_parent->m_mutex);
            while (!m_parent->m_finish && !is_our_value()) {
                m_parent->m_cv.wait(lk);
            }
        }
        {
            std::lock_guard<std::mutex> lk(m_parent->m_mutex);
            if (!inc()) {
                m_parent->m_finish = true;
            }
        }
        m_parent->m_cv.notify_all();
#endif
    }
}

template<typename Mutex>
void do_work(int num_threads, int max_value) {
    TimeLogger time_logger;
    
    Incrementers<Mutex> incrementers(num_threads, max_value);
}

void increment(int num_threads, int max_value) {
#ifdef STD_MUTEX
    // std::mutex
    // first pass
    do_work<std::mutex>(num_threads, max_value);
    // second pass
    do_work<std::mutex>(num_threads, max_value);
    // num_threads=50, max_value=1000
    // main: 13414489 micros passed
    // main: 12973986 micros passed
    // Program ended with exit code: 0
#elif defined(SPINLOCK_MUTEX)
    // SpinLockMutex
    // first pass
    do_work<SpinLockMutex>(num_threads, max_value);
    // second pass
    do_work<SpinLockMutex>(num_threads, max_value);
    // num_threads=50, max_value=1000
    // main: 12635709 micros passed
    // main: 12003058 micros passed
    // Program ended with exit code: 0
    //Program ended with exit code: 0
    //SpinLockMutex seems to perform a little better
#elif defined(WAIT_NOTIFY)
    // condition_variable
    // first pass
    do_work<std::mutex>(num_threads, max_value);
    // second pass
    do_work<std::mutex>(num_threads, max_value);
    // Results:
    // num_threads=50, max_value=1000
    // main: 39448 micros passed
    // main: 38970 micros passed
    // Program ended with exit code: 0
#endif
}
