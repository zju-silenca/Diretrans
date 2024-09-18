#include "ThreadPool.h"


ThreadPool::ThreadPool(int poolSize)
    : poolSize_(poolSize),
    running_(true)
{
    threadPool_.reserve(poolSize);
    for (int i = 0; i < poolSize; ++i)
    {
        threadPool_.emplace_back(std::thread(std::bind(&ThreadPool::threadFunc, this)));
    }
}

ThreadPool::~ThreadPool()
{
    running_ = false;
    cond_.notify_all();
    poolJoin();
}

void ThreadPool::addTask(ThreadPoolFunc f)
{
    if (!running_) return;
    std::unique_lock<std::mutex> lock(mutex_);
    tasks_.push_back(f);
    cond_.notify_one();
}

void ThreadPool::addUrgentTask(ThreadPoolFunc f)
{
    if (!running_) return;
    std::unique_lock<std::mutex> lock(mutex_);
    tasks_.push_front(f);
    cond_.notify_one();
}

void ThreadPool::threadFunc()
{
    auto threadId = std::this_thread::get_id();
    std::stringstream threadIdStr;
    threadIdStr << std::hex << threadId;
    LOG("Thread 0x%s created.", threadIdStr.str().c_str());
    while (running_)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock);

        while (!tasks_.empty())
        {
            ThreadPoolFunc fc = *tasks_.begin();
            tasks_.pop_front();
            lock.unlock();
            fc();
            lock.lock();
        }
        lock.unlock();
    }
    LOG("Thread 0x%s exit.", threadIdStr.str().c_str());
}

void ThreadPool::poolJoin()
{
    for (int i = 0 ; i < poolSize_; ++i)
    {
        cond_.notify_all();
        threadPool_[i].join();
    }
}

