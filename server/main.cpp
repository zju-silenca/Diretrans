#include <pthread.h>
#include <sys/timerfd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "netlib/Log.h"
#include "netlib/EventLoop.h"
#include "netlib/UdpCom.h"

EventLoop* g_loop;

void Timeout()
{
    LOG("Timeout!");
    //g_loop->quit();
}

void print_test(const char* msg)
{
    LOG("%s", msg);
}

void* testFunc(void *)
{
    g_loop->runAfter(1.5, std::bind(print_test, "once1"));
    return nullptr;
}

void handleUdp(UdpCom* conn, Buffer& buf, struct sockaddr& sourceAddr, Timestamp& time)
{
    char ip[INET_ADDRSTRLEN];
    uint16_t port;

    std::string msg(buf.peek(), buf.readableBytes());
    struct sockaddr_in *sa_in = (struct sockaddr_in *)&sourceAddr;

    inet_ntop(AF_INET, &sa_in->sin_addr, ip, INET_ADDRSTRLEN);
    port = ntohs(sa_in->sin_port);

    LOG("[%s from %s:%u] %s",
        time.toFormateString().c_str(),
        ip, port, msg.c_str());
    if (conn)
    {
        conn->send(buf, sourceAddr);
    }
    else
    {
        LOG("Conn has been destroyed.");
    }
}

int main()
{
    EventLoop loop;
    pthread_t threadId;
    g_loop = &loop;

    loop.runAfter(1, std::bind(print_test, "once1"));
    pthread_create(&threadId, nullptr, testFunc, nullptr);
    // loop.runAfter(1.5, std::bind(print_test, "once1.5"));
    // loop.runAfter(2.5, std::bind(print_test, "once2.5"));
    // loop.runAfter(3.5, std::bind(print_test, "once3.5"));
    // loop.runEvery(2, std::bind(print_test, "every2"));
    // loop.runEvery(3, std::bind(print_test, "every3"));
    auto conn = std::make_shared<UdpCom>(&loop, 19987);
    conn->setMessageCallback(handleUdp);

    loop.loop();
    LOG("main exit.");
    return 0;
}