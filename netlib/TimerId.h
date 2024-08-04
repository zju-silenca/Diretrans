#ifndef _TIMERID_H_
#define _TIMERID_H_

#include "Timer.h"

class TimerId
{
public:
    TimerId()
    : value_(nullptr)
    {}

    explicit TimerId(std::shared_ptr<Timer> timer)
    : value_(timer)
    { }

    TimerId(TimerId&& other) noexcept
    : value_(std::move(other.value_))
    {}

    // 移动赋值运算符
    TimerId& operator=(TimerId&& other) noexcept
    { value_ = std::move(other.value_); return *this; }


    void cancel() { value_.reset(); }

private:
    std::shared_ptr<Timer> value_;
};

#endif