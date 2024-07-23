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
    typedef std::pair<Timestamp, std::shared_ptr<Timer>> Entry;
    typedef std::set<Entry> TimerList;

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
