#ifndef _MUTEXLOCK_H_
#define _MUTEXLOCK_H_

#include <mutex>

class MutexLock
{
public:
    MutexLock()
    {
        pthread_mutex_init(&mutex_, nullptr);
    }

    ~MutexLock()
    {
        pthread_mutex_destroy(&mutex_);
    }

    void lock(){ pthread_mutex_lock(&mutex_); }
    void unlock(){ pthread_mutex_unlock(&mutex_); }

private:
    pthread_mutex_t mutex_;
};

class MutexLockGuard
{
public:
    MutexLockGuard(MutexLock lock) : lock_(lock) { lock_.lock(); }
    ~MutexLockGuard() { lock_.unlock(); }

private:
    MutexLock lock_;
};

#endif