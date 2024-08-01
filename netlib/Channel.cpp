#include "Channel.h"
#include "EventLoop.h"
#include "Log.h"
#include <sys/epoll.h>
#include <sys/poll.h>
#include <cassert>

static_assert(EPOLLIN == POLLIN);
static_assert(EPOLLPRI == POLLPRI);
static_assert(EPOLLOUT == POLLOUT);

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd)
 :  loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    eventHandling_(false)
{
}

Channel::~Channel()
{
    assert(!eventHandling_);
}

void Channel::handleEvent()
{
    eventHandling_ = true;
    if (revents_ & POLLNVAL)
    {
        LOG("Channel::handleEvent POLLNVAL");
    }

    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        LOG("Channel::handle_event() POLLHUP");
        if (closeCallback_) closeCallback_();
    }

    if (revents_ & (POLLERR | POLLNVAL))
    {
        if (errorCallback_) errorCallback_();
    }

    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
    {
        if (readCallback_) readCallback_();
    }

    if (revents_ & (POLLOUT))
    {
        if (writeCallback_) writeCallback_();
    }
    eventHandling_ = false;
}

void Channel::remove()
{
    assert(isNoneEvent());
    loop_->removeChannel(this);
}

void Channel::update()
{
    loop_->updateChannel(this);
}
