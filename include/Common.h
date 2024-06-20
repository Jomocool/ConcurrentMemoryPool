#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <vector>
#include <assert.h>
#include <thread>
using std::cout;
using std::endl;
using std::vector;

static const size_t FREE_LIST_NUM = 208;    // 哈希表中自由链表的个数
static const size_t MAX_BYTES = 256 * 1024; // ThreadCache单次申请的最大字节数

/// @brief obj的一个指针大小的字节
/// @details 用static修饰，防止多个.cpp文件重复包含该Common头文件导致链接时产生冲突
/// @return 这里返回引用是为了返回一个左值，这样才能修改。如果返回一个指针，就是一个无法修改的右值
static void *&ObjNext(void *obj)
{
    return *(void **)obj;
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
        _freeList = obj;
    }

    // 提供空间
    void *pop()
    {
        // 头删
        assert(_freeList); // 提供空间的前提是要有空间
        void *obj = _freeList;
        _freeList = ObjNext(obj);
        return obj;
    }

    // 判断是否为空
    bool empty()
    {
        return _freeList == nullptr;
    }

private:
    void *_freeList = nullptr; // 自由链表，初始为空
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

#endif