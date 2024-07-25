#include "EventLoop.h"
#include "EPoller.h"
#include "Channel.h"
#include "TimerQueue.h"
#include "TimerId.h"
#include "Timer.h"
#include <pthread.h>
#include <cassert>
#include <sys/eventfd.h>

__thread EventLoop* t_loopInThisThread = nullptr;

static const int kPollTimeoutMs = 10000;

static int createWakeupFd()
{
    int evefd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evefd < 0)
    {
        LOG("createWakeupFd failed.");
        abort();
    }
    return evefd;
}

EventLoop::EventLoop() :
    looping_(false),
    quit_(false),
    threadId_(syscall(SYS_gettid)),
    poller_(new EPoller(this)),
    timerQueue_(new TimerQueue(this)),
    callingPendingFunc_(false),
    wakeupFd_(createWakeupFd()),
    wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG("EventLoop created %p in thread %d", this, threadId_);

    if(t_loopInThisThread)
    {
        LOG("There is loop %p in this thread %d.", t_loopInThisThread, threadId_);
        abort();
    }else
    {
        t_loopInThisThread = this;
    }
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    assert(!looping_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    LOG("EventLoop %p start.", this);
    while (!quit_)
    {
        activeChannels_.clear();
        poller_->poll(kPollTimeoutMs, &activeChannels_);
        for (ChannelList::iterator it = activeChannels_.begin();
            it != activeChannels_.end(); ++it)
        {
            (*it)->handleEvent();
        }
        doPendingFunctors();
    }

    LOG("EventLoop %p stop.", this);
    looping_ = false;
}

void EventLoop::assertInLoopThread()
{
    if (!isInLoopThread())
    {
        LOG("assertInLoopThread failed!");
        abort();
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::queueInLoop(const Functors &cb)
{
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.push_back(cb);
    }

    if (!isInLoopThread() || callingPendingFunc_)
    {
        wakeup();
    }
}

void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {
        wakeup();
    }
}

void EventLoop::runInLoop(const Functors &cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(cb);
    }
}

TimerId EventLoop::runAt(const Timestamp &time, const TimerCallback &cb)
{
    return timerQueue_->addTimer(cb, time, 0);
}

TimerId EventLoop::runAfter(double delaySec, const TimerCallback &cb)
{
    Timestamp time(addTime(Timestamp::now(), delaySec));
    return runAt(time, cb);
}

TimerId EventLoop::runEvery(double intervalSec, const TimerCallback &cb)
{
    Timestamp time(addTime(Timestamp::now(), intervalSec));
    return timerQueue_->addTimer(cb, time, intervalSec);
}

void EventLoop::handleRead()
{
    uint64_t tmp = 1;
    ssize_t n = ::read(wakeupFd_, &tmp, sizeof(tmp));
    if (n != sizeof(tmp))
    {
        LOG("EventLoop::handleRead() writes %ld bytes", n);
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functors> functors;
    callingPendingFunc_ = true;

    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i)
    {
        functors[i]();
    }
    callingPendingFunc_ = false;
}

void EventLoop::wakeup()
{
    uint64_t tmp = 1;
    ssize_t n = ::write(wakeupFd_, &tmp, sizeof(tmp));
    if (n != sizeof(tmp))
    {
        LOG("EventLoop::wakeup() writes %ld bytes", n);
    }
}
