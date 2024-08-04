#ifndef _EVENT_LOOP_H_
#define _EVENT_LOOP_H_

#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory>
#include <vector>
#include "Callback.h"
#include "TimerId.h"
#include "Mutexlock.h"
#include "Log.h"

class Channel;
class EPoller;
class Timestamp;
class TimerQueue;

class EventLoop
{
public:
    EventLoop();
    ~EventLoop();
    // noncopyable
    EventLoop(const EventLoop&) = delete;

    void loop();
    void assertInLoopThread();
    bool isInLoopThread() { return threadId_ == syscall(SYS_gettid);}
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    void queueInLoop(const Functors& cb);
    void quit();
    void runInLoop(const Functors& cb);

    /*
    TimerId must keep live when u want it work.
    If u want cancel, just destroy TimerId.
    */
    [[nodiscard]] TimerId runAt(const Timestamp& time, const TimerCallback& cb);
    [[nodiscard]] TimerId runAfter(double delaySec, const TimerCallback& cb);
    [[nodiscard]] TimerId runEvery(double intervalSec, const TimerCallback& cb);

private:
    void handleRead(); // weak up
    void doPendingFunctors();
    void wakeup();

    typedef std::vector<Channel*> ChannelList;

    bool looping_;
    bool quit_;
    bool callingPendingFunc_;
    const pid_t threadId_;
    std::unique_ptr<EPoller> poller_;
    ChannelList activeChannels_;
    std::unique_ptr<TimerQueue> timerQueue_;
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;
    std::vector<Functors> pendingFunctors_;
    MutexLock mutex_;
};

#endif