#ifndef _POLLER_H_
#define _POLLER_H_

#include <sys/epoll.h>
#include <vector>
#include <map>
#include "EventLoop.h"

struct pollfd;
class Channel;

class Poller{
public:
    typedef std::vector<Channel*> ChannelList;
    Poller(EventLoop* loop);
    ~Poller();
    Poller(const Poller&) = delete;

    int poll(int timeoutMs, ChannelList* activeChannels);

    void updateChannel(Channel* channel);

    void assertInLoopThread() { loop_->assertInLoopThread(); }

private:
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    typedef std::vector<struct pollfd> PollFdList;
    typedef std::map<int, Channel*> ChannelMap;

    EventLoop* loop_;
    PollFdList pollfds_;
    ChannelMap channels_;
};

#endif