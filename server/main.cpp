#include <pthread.h>
#include "netlib/Log.h"
#include "DiretransServer.h"

int main()
{
    DiretransServer server(19987);
    server.start();
    LOG("main exit.");
    return 0;
}