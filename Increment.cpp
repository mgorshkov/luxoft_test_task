//
//  Increment.cpp
//  luxoft_test_task
// 1.Use two threads to increment an integer. Thread A increments when even and Thread B increments when odd (for the integer problem we can have it specify up to a number provided on the command line)
// 1a. What are some of the difficulties in adding more threads? Please show the difficulties with code.
// 1b. Extra credit – Design an improved solution to the above that can scale with many threads

// I have considered 4 different solutions:
// * std::mutex
// * lock-free mutex
// * Wait-notify approach with std::condition_variable
// * direct lock-free increment
// For each solution, thread pinning is available
// Out of these 4 approaches, we get the best performance and scaling with lock-free increment and thread pinning.
// Some problems with mutexes approach:
// 1) performance significantly drops as number of threads increases
// 2) CPU consumption increases with number of threads
// Reasons:
// 1) threads waste time on waiting since it adds some overhead
// 2) the scheduler won't give them time quants when waiting thus not allowing to work when wait is close to its completion

/* 
Lock-free increment with thread pinning
./luxoft_test_task increment 10 10000
main: 51041 micros passed
main: 51580 micros passed

std::mutex without thread pinning
./luxoft_test_task increment 10 10000
main: 61977 micros passed
main: 60117 micros passed
*/

//  Created by Mikhail Gorshkov on 21.08.2021.
//

#include <list>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <cstring>
#include <sched.h>

//#define DEBUG_LOG

//#define MODE STD_MUTEX
//#define MODE SPINLOCK_MUTEX
//#define MODE WAIT_NOTIFY
#define MODE LOCK_FREE

#define PIN_THREADS_TO_CPU

#include "TimeLogger.hpp"

static int pin_to_cpu(pthread_t thread_handle, int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(thread_handle, sizeof(cpuset), &cpuset);
}

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
    bool atomic_inc();
    
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
#if MODE == WAIT_NOTIFY
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
#ifdef PIN_THREADS_TO_CPU
    static const unsigned int processor_count = std::thread::hardware_concurrency();
    const int cpu = m_num_thread % processor_count;
    int res = pin_to_cpu(m_thread.native_handle(), cpu);
#ifdef DEBUG_LOG
    std::cout << "Thread pinned to CPU " << cpu << " successfully" << std::endl;
#endif
#endif
}
    
template<typename Mutex>
Incrementer<Mutex>::~Incrementer() {
    m_thread.join();
}
    
template<typename Mutex>
bool Incrementer<Mutex>::inc() {
    if (m_parent->m_value.load() > m_max_value) {
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
bool Incrementer<Mutex>::atomic_inc() {
    int new_value;
    int old_value = m_parent->m_value.load(std::memory_order_relaxed);
    do
    {
       if (old_value % m_total_threads != m_num_thread)
           return true;
       if (old_value > m_max_value)
	   return false;
       new_value = old_value + 1;
    } while (!m_parent->m_value.compare_exchange_weak(old_value, new_value, std::memory_order_relaxed,
       std::memory_order_relaxed));
#ifdef DEBUG_LOG
    std::cout << "m_value=" << m_parent->m_value << ", m_num_thread=" << m_num_thread << std::endl;
#endif
    return true;
}

template<typename Mutex>
bool Incrementer<Mutex>::is_our_value() const {
    return m_parent->m_value % m_total_threads == m_num_thread;
}

template<typename Mutex>
void Incrementer<Mutex>::thread_proc() {
    while (!m_parent->m_finish.load(std::memory_order_relaxed)) {
#if MODE == STD_MUTEX || MODE == SPINLOCK_MUTEX
        std::lock_guard<Mutex> lk(m_parent->m_mutex);
        if (!is_our_value()) {
            continue;
        }
        if (!inc()) {
            m_parent->m_finish.store(true, std::memory_order_relaxed);
        }
#elif MODE == WAIT_NOTIFY
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
#elif MODE == LOCK_FREE
	if (!atomic_inc()) {
	    m_parent->m_finish = true;
        }
#endif
    }
}

template<typename Mutex>
void do_work(int num_threads, int max_value) {
    TimeLogger time_logger;
    
    Incrementers<Mutex> incrementers(num_threads, max_value);
}

void increment(int num_threads, int max_value) {
#if MODE == STD_MUTEX || MODE == WAIT_NOTIFY || MODE == LOCK_FREE
    // std::mutex
    // first pass
    do_work<std::mutex>(num_threads, max_value);
    // second pass
    do_work<std::mutex>(num_threads, max_value);
#elif MODE == SPINLOCK_MUTEX
    // SpinLockMutex
    // first pass
    do_work<SpinLockMutex>(num_threads, max_value);
    // second pass
    do_work<SpinLockMutex>(num_threads, max_value);
#endif
}
