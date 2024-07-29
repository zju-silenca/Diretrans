#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include "DiretransClient.h"

static bool threadRunning = true;

void* sendCmd(void* arg)
{
    char buf[1024];
    DiretransClient* client = static_cast<DiretransClient*>(arg);
    LOG("start!!!!!!");
    while (threadRunning)
    {
        std::cout << "Input cmd:";
        if (fgets(buf, sizeof(buf), stdin))
        {
            size_t len = strlen(buf);
            if (buf[len - 1] == '\n')
            {
                buf[len - 1] = '\0';
            }
        }
        if (0 > client->writeCmdtoSock(buf, strlen(buf)))
        {
            LOG("Write error. errno:%d", errno);
        }
        memset(buf, 0, sizeof(buf));
        assert(strlen(buf) == 0);
    }
    return nullptr;
}

int main()
{
    pthread_t cmdThreadId;
    DiretransClient client;

    if (0 != pthread_create(&cmdThreadId, nullptr, sendCmd, static_cast<void*>(&client)))
    {
        LOG("create thread error.");
    }
    client.start();
    threadRunning = false;
    LOG("Main thread exit, please input enter.");
    pthread_join(cmdThreadId, nullptr);
    return 0;
}