#ifndef _TIMERQUEUE_H_
#define _TIMERQUEUE_H_

#include <memory>
#include <set>
#include <vector>
#include "Callback.h"
#include "Timestamp.h"
#include "Channel.h"

class EventLoop;
class Timer;
class TimerId;

class TimerQueue
{
public:
    TimerQueue(EventLoop* loop);
    ~TimerQueue() = default;


    TimerId addTimer(const TimerCallback& cb,
        Timestamp when,
        double interval);

    void cancel(TimerId& timerId);

    TimerQueue(const TimerQueue&) = delete;

private:
    struct TimerComparator {
        bool operator()(const std::pair<Timestamp, std::weak_ptr<Timer>>& lhs,
                        const std::pair<Timestamp, std::weak_ptr<Timer>>& rhs) const {
            // 仅比较 Timestamp 部分，忽略 weak_ptr
            return lhs.first < rhs.first;
        }
    };
    typedef std::pair<Timestamp, std::weak_ptr<Timer>> Entry;
    typedef std::set<Entry, TimerComparator> TimerList;

    void addTimerInLoop(std::shared_ptr<Timer> timer);

    void handleRead();
    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);
    bool insert(std::shared_ptr<Timer> timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerList timers_;
};

#endif
