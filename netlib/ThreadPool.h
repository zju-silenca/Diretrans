#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <thread>
#include <vector>
#include <deque>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "Callback.h"
#include "Log.h"

class ThreadPool
{
public:
    using ThreadPoolFunc = Functors;

    ThreadPool(int poolSize);
    ~ThreadPool();
    int getPoolSize() const { return poolSize_; }
    void addTask(ThreadPoolFunc f);
    void addUrgentTask(ThreadPoolFunc f);
private:
    const int poolSize_;
    bool running_;
    std::vector<std::thread> threadPool_;
    std::deque<ThreadPoolFunc> tasks_;
    std::condition_variable cond_;
    std::mutex mutex_;

    void threadFunc();
    void poolJoin();
};

#endif