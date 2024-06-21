#include "../include/CentralCache.h"

CentralCache CentralCache::_sInst; // CentralCache的饿汉对象

Span *CentralCache::GetOneSpan(SpanList &list, size_t size)
{
    return nullptr;
}

size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t size)
{
    // 获取到size对应到哪一个SpanList
    size_t index = SizeClass::Index(size);

    // 由于cc是全局唯一的，因此对桶中自由链表操作时要加锁
    _spanLists[index]._mtx.lock();

    // 获取一个管理空间不为空的Span
    Span *span = GetOneSpan(_spanLists[index], size);
    assert(span);
    assert(span->_freeList);

    start = end = span->_freeList; // 初始化start、end指向第一块
    int i = 1, actualNum = 1;      // 已经指向第一块了，所以end起始位移和实际分配块数都是1
    while (i < batchNum - 1 && ObjNext(end) != nullptr)
    {
        end = ObjNext(end); // 后移end
        ++i;                // end位移+1
        ++actualNum;        // 实际分配块数+1
    }
    span->_freeList = ObjNext(end); // 更新自由链表头节点
    ObjNext(end) = nullptr;         // 将分配的块和原先的自由链表断开

    _spanLists[index]._mtx.unlock();
}