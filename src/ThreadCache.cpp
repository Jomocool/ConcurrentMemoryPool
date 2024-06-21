#include "../include/ThreadCache.h"

/// @brief 线程申请size大小的空间
void *ThreadCache::Allocate(size_t size)
{
    assert(size < MAX_BYTES); // 线程单次不能申请超过256KB的空间

    size_t alignSize = SizeClass::RoundUp(size); // 拿到对齐后的大学
    size_t index = SizeClass::Index(size);       // 拿到对应桶下标

    if (!_freeLists[index].empty())
    { // 自由链表不为空，直接从链表中获取空间
        return _freeLists[index].pop();
    }
    else
    { // 自由链表为空，向CentralCache中申请空间
        return FetchFromCentralCache(index, alignSize);
    }
}

/// @brief 回收线程大小为size的obj空间
void ThreadCache::Deallocate(void *obj, size_t size)
{
    assert(obj);               // 回收空间不能为空
    assert(size <= MAX_BYTES); // 回收空间不能超过256KB

    // size可以保证已经是对齐的
    size_t index = SizeClass::Index(size); // 找到对应自由链表的下标
    _freeLists[index].push(obj);           // 用对应自由链表回收obj
}

/// @brief ThreadCache空间不够时，向CentralCache申请空间
void *ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize)
{
    // 通过MaxSize和NumMoveSize来控制当前分配的块数
    size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize)); // 取最小值是防止MaxSize一直递加过大

    // 当前_maxSize没有达到上限，仍可增加
    if (batchNum == _freeLists[index].MaxSize())
        _freeLists[index].MaxSize()++;

    // 上面就是慢开始反馈算法

    // 因为返回的空间大小可能不满足需求，所以需要记录返回空间的起始、结束地址
    void *start = nullptr;
    void *end = nullptr;
    int actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, alignSize);

    assert(actualNum >= 1);

    // 不需要push到tc的自由链表了
    if (actualNum == 1)
        assert(start == end);
    else
        _freeLists[index].pushRange(ObjNext(start), end); // 将多余块放入自由链表中

    // 返回第一块给线程
    return start;
}