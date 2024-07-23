#ifndef _TIMESTAMP_H_
#define _TIMESTAMP_H_

#include <stdint.h>
#include <string>

class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSeconds);

    void swap(Timestamp& stamp) { std::swap(microSeconds_, stamp.microSeconds_); }

    std::string toString() const;
    std::string toFormateString() const;

    bool valid() const {return microSeconds_ > 0;}

    int64_t getMicroSeconds() { return microSeconds_; }

    static Timestamp now();

    static Timestamp invalid();

    static const int64_t kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t microSeconds_;
};

inline bool operator<(Timestamp l, Timestamp r)
{
    return l.getMicroSeconds() < r.getMicroSeconds();
}

inline bool operator==(Timestamp l, Timestamp r)
{
    return l.getMicroSeconds() == r.getMicroSeconds();
}

inline Timestamp addTime(Timestamp timestamp, double seconds)
{
  int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.getMicroSeconds() + delta);
}

#endif