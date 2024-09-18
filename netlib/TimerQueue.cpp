#include "TimerQueue.h"
#include "Timer.h"
#include "TimerId.h"
#include "EventLoop.h"

#include <sys/timerfd.h>
#include <cassert>

static struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.getMicroSeconds() - Timestamp::now().getMicroSeconds();

    if (microseconds < 100)
    {
        // why?
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<time_t>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);

    return ts;
}

static int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG("Create timerfd error.");
        abort();
    }
    return timerfd;
}

static void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
    LOG("TimerQueue::handleRead() %ld at %s", howmany, now.toString().c_str());

    if (n != sizeof(howmany))
    {
        LOG("TimerQueue::handleRead() reads %ld bytes instead of 8.", n);
    }
}

static void resetTimerfd(int timerfd, Timestamp expiration)
{
    struct itimerspec newVal = {0};
    struct itimerspec oldVal = {0};

    newVal.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newVal, &oldVal);
    if (ret != 0)
    {
        LOG("timerfd_settime error.");
    }
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_()
{
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enableReading();
}

TimerId TimerQueue::addTimer(const TimerCallback &cb, Timestamp when, double interval)
{
    std::shared_ptr<Timer> timer = std::make_shared<Timer>(cb, when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer);
}

void TimerQueue::cancel(TimerId& timerId)
{
    timerId.cancel();
}

void TimerQueue::addTimerInLoop(std::shared_ptr<Timer> timer)
{
    bool earliestChanged = insert(timer);
    if (earliestChanged)
    {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    std::vector<Entry> expired = getExpired(now);

    for (std::vector<Entry>::iterator it = expired.begin();
        it != expired.end(); )
    {
        auto timer = it->second.lock();
        if (timer != nullptr)
        {
            timer->run();
            ++it;
        }
        else
        {
            it = expired.erase(it);
        }
    }

    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> expired;
    std::weak_ptr<Timer> tmp;
    Entry temp = std::make_pair(now, tmp);
    TimerList::iterator it = timers_.lower_bound(temp);
    assert(it == timers_.end() || now < it->first);
    std::copy(timers_.begin(), it, back_inserter(expired));
    timers_.erase(timers_.begin(), it);

    return expired;
}

void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
    Timestamp nextExpire;

    for (std::vector<Entry>::const_iterator it = expired.begin();
        it != expired.end(); ++it)
    {
        auto timer = it->second.lock();
        if (timer != nullptr && timer->repeat())
        {
            timer->restart(now);
            insert(timer);
        }
    }

    while (!timers_.empty())
    {
        auto timer = timers_.begin()->second.lock();
        if (timer != nullptr)
        {
            nextExpire = timer->expiration();
            break;
        }else
        {
            timers_.erase(timers_.begin());
        }
    }

    if (nextExpire.valid())
    {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(std::shared_ptr<Timer> timer)
{
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }

    std::pair<TimerList::iterator, bool> result = 
        timers_.insert(std::make_pair(when, std::weak_ptr<Timer>(timer)));
    assert(result.second); (void)result;
    return earliestChanged;
}
