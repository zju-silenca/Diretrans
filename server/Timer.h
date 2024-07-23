#ifndef _TIMER_H_
#define _TIMER_H_

#include "Callback.h"
#include "Timestamp.h"
#include "Log.h"

class Timer
{
public:
    Timer(const TimerCallback& cb, Timestamp when, double interval)
    :   cb_(cb),
        expiration_(when),
        interval_(interval),
        repeat_(interval_ > 0.0)
    {}
    Timer(const Timer& ) = delete;

    void run() const
    {
        if (cb_)
        {
            cb_();
        }
    }

    Timestamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }

    void restart(Timestamp now)
    {
        if (repeat_)
        {
            expiration_ = addTime(now, interval_);
        } else
        {
            expiration_ = Timestamp::invalid();
        }
    }

private:
    const TimerCallback cb_;
    // 到期时间
    Timestamp expiration_;
    // repeat间隔
    const double interval_;
    const bool repeat_;
};

#endif