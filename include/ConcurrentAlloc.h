#ifndef CONCURRENT_ALLOC_H
#define CONCURRENT_ALLOC_H

#include "ThreadCache.h"

/// @brief 线程申请空间的函数
void *ConcurrentAlloc(size_t size)
{
    cout << std::this_thread::get_id() << " " << pTLSThreadCache << endl;

    // 由于每个线程都有一个独立的全局对象，因此不存在线程安全问题
    if (pTLSThreadCache == nullptr)
    {
        pTLSThreadCache = new ThreadCache;
    }

    return pTLSThreadCache->Allocate(size);
}

/// @brief 线程回收空间的函数
void ConcurrentFree(void *obj, size_t size)
{
    assert(obj);
    pTLSThreadCache->Deallocate(obj, size);
}

#endif