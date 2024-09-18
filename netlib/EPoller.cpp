#include "EPoller.h"
#include "Channel.h"
#include <poll.h>
#include <cassert>

#define MAX_EVENTS (64)

static int createEpollFd()
{
    int fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (fd <= 0)
    {
        LOG("Create epoll fd error.");
        abort();
    }
    return fd;
}

EPoller::EPoller(EventLoop *loop)
 :  loop_(loop),
  epollfd_(createEpollFd())
{
}

EPoller::~EPoller()
{
    if (epollfd_ > 0)
    {
        ::close(epollfd_);
    }
}

int EPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    struct epoll_event events[MAX_EVENTS];
    int numEvents = ::epoll_wait(epollfd_, events, MAX_EVENTS, timeoutMs);

    if (numEvents > 0)
    {
        //LOG("%d events happended.", numEvents);
        fillActiveChannels(numEvents, events, activeChannels);
    } else if (numEvents == 0)
    {
        LOG("Nothing happended.");
    } else
    {
        LOG("poll error.");
    }
    return 0;
}

void EPoller::updateChannel(Channel *channel)
{
    assertInLoopThread();
    LOG("fd = %d, events = %d", channel->fd(), channel->events());
    if (channel->index() < 0)
    {
        // new channel
        assert(channels_.find(channel->fd()) == channels_.end());
        struct epoll_event ev;
        ev.data.fd = channel->fd();
        ev.events = static_cast<uint32_t>(channel->events());
        pollfds_.push_back(ev);
        int idx = static_cast<int>(pollfds_.size() - 1);
        channel->set_index(idx);
        channels_[ev.data.fd] = channel;
        ::epoll_ctl(epollfd_, EPOLL_CTL_ADD, ev.data.fd, &ev);
    } else
    {
        // update this fd
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);
        int idx = channel->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
        struct epoll_event& ev = pollfds_[idx];
        assert(ev.data.fd == channel->fd() || ev.data.fd == -channel->fd()-1);
        ev.events = static_cast<uint32_t>(channel->events());

        if (channel->isNoneEvent())
        {
            ::epoll_ctl(epollfd_, EPOLL_CTL_DEL, ev.data.fd, &ev);
            // 后续可能需要重新添加此fd
            ev.data.fd = -channel->fd()-1;
        }else
        {
            ::epoll_ctl(epollfd_, EPOLL_CTL_MOD, ev.data.fd, &ev);
        }
    }
}

void EPoller::removeChannel(Channel *channel)
{
    assertInLoopThread();
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    assert(channel->isNoneEvent());
    int idx = channel->index();
    assert(0 <= idx && idx <= static_cast<int>(pollfds_.size()));
    const struct epoll_event& ev = pollfds_[idx]; (void)ev;
    assert(ev.data.fd == -channel->fd()-1 &&  static_cast<int>(ev.events) == channel->events());
    size_t n = channels_.erase(channel->fd());
    assert(n == 1); (void)n;
    if (static_cast<size_t>(idx) == pollfds_.size() - 1)
    {
        pollfds_.pop_back();
    }
    else
    {
        int channelAtEnd = pollfds_.back().data.fd;
        std::iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
        if (channelAtEnd < 0)
        {
            channelAtEnd = -channelAtEnd-1;
        }
        channels_[channelAtEnd]->set_index(idx);
        pollfds_.pop_back();
    }
}

void EPoller::fillActiveChannels(int numEvents, struct epoll_event* events, ChannelList* activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        struct epoll_event& ev = events[i];
        if (ev.events > 0)
        {
            ChannelMap::const_iterator ch = channels_.find(ev.data.fd);
            assert(ch != channels_.end());
            Channel* channel = ch->second;
            assert(channel->fd() == ev.data.fd);
            channel->set_revents(ev.events);
            activeChannels->push_back(channel);
        }
    }
}
