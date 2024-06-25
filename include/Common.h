#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <vector>
#include <assert.h>
#include <thread>
#include <mutex>
using std::cout;
using std::endl;
using std::vector;

typedef size_t PageID;

static const size_t FREE_LIST_NUM = 208;    // 哈希表中自由链表的个数
static const size_t MAX_BYTES = 256 * 1024; // ThreadCache单次申请的最大字节数
static const size_t PAGE_NUM = 128;         // span的最大管理页数
static const size_t PAGE_SHIFT = 12;        // 一页4KB，12位

/// @brief obj的一个指针大小的字节
/// @details 用static修饰，防止多个.cpp文件重复包含该Common头文件导致链接时产生冲突
/// @return 这里返回引用是为了返回一个左值，这样才能修改。如果返回一个指针，就是一个无法修改的右值
static void *&ObjNext(void *obj)
{
    return *(void **)obj;
}

/// @brief 向系统申请k页内存空间
void *SystemAlloc(size_t k)
{
    void *ptr = malloc(k << PAGE_SHIFT);
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}

class FreeList
{
public:
    // 回收空间
    void push(void *obj)
    {
        // 头插
        assert(obj); // 插入非空空间
        ObjNext(obj) = _freeList;

        ++_size;

        _freeList = obj;
    }

    // 插入多块空间
    void pushRange(void *start, void *end, size_t size)
    {
        ObjNext(end) = _freeList;
        _freeList = start;

        _size += size;
    }

    // 提供空间
    void *pop()
    {
        // 头删
        assert(_freeList); // 提供空间的前提是要有空间
        void *obj = _freeList;
        _freeList = ObjNext(obj);

        --_size;

        return obj;
    }

    void PopRange(void *&start, void *&end, size_t n)
    {
        assert(n <= _size);

        start = end = _freeList;
        for (int i = 0; i < n - 1; ++i)
        {
            end = ObjNext(end);
        }

        _freeList = ObjNext(end);
        ObjNext(end) = nullptr;
        _size -= n;
    }

    // 判断是否为空
    bool empty()
    {
        return _freeList == nullptr;
    }

    // FreeList当前未到上限时，能够申请的最大块空间是多少
    size_t &MaxSize()
    {
        return _maxSize;
    }

    size_t size()
    {
        return _size;
    }

private:
    void *_freeList = nullptr; // 自由链表，初始为空
    size_t _maxSize = 1;       // 当前自由链表申请未达到上限时，能够申请的最大空间块数
    size_t _size = 0;          // 当前自由链表的块数量
};

/// @brief 计算线程申请的空间大小对齐后的字节数
class SizeClass
{
public:
    /// @brief 计算size对齐后的大小
    static size_t RoundUp(size_t size)
    {
        assert(size <= MAX_BYTES);

        size_t alignNum = 0;
        if (size <= 128)
            alignNum = 8;
        else if (size <= 1024)
            alignNum = 16;
        else if (size <= 8 * 1024)
            alignNum = 128;
        else if (size <= 64 * 1024)
            alignNum = 1024;
        else if (size <= 256 * 1024)
            alignNum = 8 * 1024;

        return _RoundUp(size, alignNum);
    }

    /// @brief 计算size对应到的桶下标
    static inline size_t Index(size_t size)
    {
        assert(size <= MAX_BYTES);

        size_t res = -1;
        static int group_array[4] = {16, 52, 52, 52}; // 前4个区间的各区间链表数
        if (size <= 128)
            res = _Index(size, 3);
        else if (size <= 1024)
            res = _Index(size - 128, 4) + group_array[0];
        else if (size <= 8 * 1024)
            res = _Index(size - 1024, 7) + group_array[1] + group_array[0];
        else if (size <= 64 * 1024)
            res = _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
        else if (size <= 256 * 1024)
            res = _Index(size - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];

        return res;
    }

    /// @brief 计算块空间大小为size时，单次能够申请的最大块数
    static size_t NumMoveSize(size_t size)
    {
        assert(size > 0);

        // 计算块数
        int num = MAX_BYTES / size;

        // 保证单次分配的块数控制在[2,512]
        if (num > 512)
            num = 512;
        else if (num < 2)
            num = 2;

        return num;
    }

    /// @brief 块页匹配算法
    static size_t NumMovePage(size_t size)
    {
        size_t num = NumMoveSize(size); // 最大分配块数
        size_t npage = num * size;      // 单次申请的最大空间
        npage >> PAGE_SHIFT;            // 页数
        return npage == 0 ? 1 : npage;  // 至少分配一页，不足一页时也分配一页
    }

private:
    static inline size_t _Index(size_t size, size_t align_shift)
    {
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
    }

    static size_t _RoundUp(size_t size, size_t alignNum)
    {
        return ((size + alignNum - 1) & ~(alignNum - 1));
    }
};

/// @brief 以页为基本单位的结构体
struct Span
{
    PageID _pageID = 0;        // 页号
    size_t _n;                 // 管理的页数量
    void *_freeList = nullptr; // 管理的小块空间的头节点
    size_t _use_count = 0;     // 已分配的小块空间数量
    Span *prev = nullptr;      // 前一个Span节点
    Span *next = nullptr;      // 后一个Span节点
    bool _isUse = false;       // 是否在pc中
};

class SpanList
{
public:
    SpanList()
    {
        _head = new Span;

        // 由于是双向链表，所以需要正确初始化prev、next
        _head->prev = _head;
        _head->next = _head;
    }

    /// @brief 弹出第一个Span
    Span *pop_front()
    {
        Span *front = _head->next;
        erase(front);
        return front;
    }

    /// @brief 判断是否有Span
    bool empty()
    {
        return _head == _head->next;
    }

    Span *begin()
    {
        return _head->next;
    }

    Span *end()
    {
        return _head;
    }

    /// @brief 头插
    void push_front(Span *ptr)
    {
        insert(_head, ptr);
    }

    /// @brief 在pos前插入ptr
    void insert(Span *pos, Span *ptr)
    {
        // 都不能为空
        assert(pos);
        assert(ptr);

        ptr->next = pos;
        ptr->prev = pos->prev;
        pos->prev->next = ptr;
        pos->prev = ptr;
    }

    /// @brief 删除节点
    void erase(Span *pos)
    {
        assert(pos);
        assert(pos != _head); // 不能删去哨兵头节点

        pos->prev->next = pos->next;
        pos->next->prev = pos->prev;
    }

private:
    Span *_head; // 哨兵头节点
public:
    std::mutex _mtx; // 每个CentralCache中的桶都要有一个桶锁（多线程安全）
};

#endif