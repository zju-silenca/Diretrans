#include "netlib/ThreadPool.h"
#include <random>

void printNums(int num, int sleepMs)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    std::cout << "printNums():" << num << std::endl;
}

int main()
{
    ThreadPool pool(5);
    std::random_device rd;
    std::mt19937 eg(rd());
    std::uniform_int_distribution<int> distribution(10, 1000);
    for (int i = 0; i < 50; ++i)
    {
        int sleepMs = distribution(eg);
        pool.addTask(std::bind(&printNums, i, sleepMs));
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}