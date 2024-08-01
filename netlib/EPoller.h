#ifndef _EPOLLER_H_
#define _EPOLLER_H_

#include <sys/epoll.h>
#include <vector>
#include <map>
#include "EventLoop.h"

class Channel;

class EPoller{
public:
    typedef std::vector<Channel*> ChannelList;
    EPoller(EventLoop* loop);
    ~EPoller();
    EPoller(const EPoller&) = delete;

    int poll(int timeoutMs, ChannelList* activeChannels);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    void assertInLoopThread() { loop_->assertInLoopThread(); }

private:
    void fillActiveChannels(int numEvents, struct epoll_event* events, ChannelList* activeChannels) const;

    typedef std::vector<struct epoll_event> EPollFdList;
    typedef std::map<int, Channel*> ChannelMap;

    EventLoop* loop_;
    EPollFdList pollfds_;
    ChannelMap channels_;
    int epollfd_;
};

#endif