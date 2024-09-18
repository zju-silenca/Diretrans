#include <iostream>
#include <atomic>
#include <netinet/in.h>
#include <string.h>
#include "netlib/EventLoop.h"
#include "netlib/UdpCom.h"
#include "netlib/Log.h"
#include <arpa/inet.h>

using namespace std;

atomic<int> recvBytes;

void handleMessage(UdpCom* conn, Buffer& buf, struct sockaddr& addr, Timestamp& time)
{
    recvBytes += buf.readableBytes();
    conn->send(buf, addr);
}

void printSpeed()
{
    double speedMB = 0;
    speedMB = (recvBytes / 1024 / 1024);
    LOG("Speed: %lf MB/s", speedMB);
    recvBytes.store(0);
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        cout << "Please input "<< argv[0] << " [type(s(erver) or c(lient))] [my_port] [peer_port]" << endl;
        return 0;
    }

    uint16_t port;
    uint16_t peerPort;
    EventLoop loop;
    Buffer buf;
    port = atoi(argv[2]);
    peerPort = atoi(argv[3]);
    recvBytes.store(0);
    UdpCom conn(&loop, port);
    conn.setMessageCallback(handleMessage);
    auto timer = loop.runEvery(1, printSpeed);

    if ('c' == argv[1][0])
    {
        struct sockaddr peerDddr;
        struct sockaddr_in* server_addr = (struct sockaddr_in*)&peerDddr;
        inet_pton(AF_INET, "127.0.0.1", &server_addr->sin_addr.s_addr);
        server_addr->sin_family = AF_INET;
        //server_addr->sin_addr.s_addr = INADDR_LOOPBACK;
        server_addr->sin_port = htons(peerPort);
        buf.hasWritten(1024);
        conn.send(buf, peerDddr);
    }

    loop.loop();
    return 0;
}