#include "../include/CentralCache.h"
#include "../include/PageCache.h"

CentralCache CentralCache::_sInst; // CentralCache的饿汉对象

Span *CentralCache::GetOneSpan(SpanList &list, size_t size)
{
    // 先在cc中寻找管理空间不为空的Span
    Span *it = list.begin();
    while (it != list.end())
    {
        if (it->_freeList != nullptr)
            return it;
        it = it->next;
    }

    list._mtx.unlock(); // 不需要使用该桶了，解锁

    // 到这说明cc中没有管理空间不为空的Span，需要向pc申请
    size_t k = SizeClass::NumMovePage(size); // 申请k页
    PageCache::GetInstance()->_pageMtx.lock();
    Span *span = PageCache::GetInstance()->NewSpan(k); // 返回一个 完全没有划分 的Span
    PageCache::GetInstance()->_pageMtx.unlock();

    // 开始切分span，切分成一个一个块，每个块大小为size
    char *start = (char *)(span->_pageID << PAGE_SHIFT);    // 起始地址
    char *end = (char *)(start + (span->_n << PAGE_SHIFT)); // 结束地址
    void *cur = start;
    start += size;
    while (start < end)
    {
        ObjNext(cur) = start;
        start += size;
        cur = ObjNext(cur);
    }
    ObjNext(cur) = nullptr;

    list._mtx.lock();
    // 将切分好的span交给list管理
    list.push_front(span);

    return span;
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
    span->_use_count += actualNum;
    ObjNext(end) = nullptr; // 将分配的块和原先的自由链表断开

    _spanLists[index]._mtx.unlock();

    return actualNum;
}