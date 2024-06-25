#ifndef THREAD_CACHE_H
#define THREAD_CACHE_H

#include "Common.h"
#include "CentralCache.h"

class ThreadCache
{
public:
    void *Allocate(size_t size);                                 // 线程申请size大小的空间
    void Deallocate(void *obj, size_t size);                     // 回收线程中大小为size、起始地址为obj的空间
    void *FetchFromCentralCache(size_t index, size_t alignSize); // ThreadCache空间不够时，向CentralCache申请空间的接口
    void ListTooLong(FreeList &list, size_t size);               // tc向cc归还list桶中的空间

private:
    FreeList _freeLists[FREE_LIST_NUM]; // 每个桶表示一个自由链表
};

// TLS的全局对象指针，每个线程下都有一个独立的全局对象
static __thread ThreadCache *pTLSThreadCache = nullptr;
// static修饰，避免链接冲突问题

#endif