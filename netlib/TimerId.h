#ifndef _TIMERID_H_
#define _TIMERID_H_

#include "Timer.h"

class TimerId
{
public:
    explicit TimerId(std::shared_ptr<Timer> timer)
    : value_(timer)
    { }

    TimerId(TimerId&& other) noexcept
    : value_(std::move(other.value_))
    {}

    void cancel() { value_.reset(); }
    void run() { value_->run(); }

private:
    std::shared_ptr<Timer> value_;
};

#endif