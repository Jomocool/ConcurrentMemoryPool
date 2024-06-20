#include "../include/ConcurrentAlloc.h"

void Alloc1()
{
    for (int i = 0; i < 5; ++i)
    {
        ConcurrentAlloc(6);
    }
}

void Alloc2()
{
    for (int i = 0; i < 5; ++i)
    {
        ConcurrentAlloc(7);
    }
}

void AllocTest()
{
    std::thread t1(Alloc1);
    std::thread t2(Alloc2);
    t1.join();
    t2.join();
}

int main()
{
    AllocTest();
    return 0;
}