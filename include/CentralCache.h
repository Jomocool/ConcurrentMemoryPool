#ifndef CENTRAL_CACHE_H
#define CENTRAL_CACHE_H
#include "Common.h"

class CentralCache
{
public:
    // 单例接口
    static CentralCache *GetInstance()
    {
        return &_sInst;
    }

    /// @brief CentralCache从自己的_spanLists中为ThreadCache提供所需要的块空间
    /// @param start 提供的空间的第一块起始地址（输出型参数）
    /// @param end 提供的空间的最后一块起始地址（输出型参数）
    /// @param n 块数
    /// @param size 单块空间的大小
    /// @return 分配的总块数
    size_t FetchRangeObj(void *&start, void *&end, size_t n, size_t size);

    // 获取一个管理空间不为空的Span
    Span *GetOneSpan(SpanList &list, size_t size);

private:
    // 隐藏构造、拷贝构造、赋值构造函数
    CentralCache() {};

    CentralCache(CentralCache &copy) = delete;
    CentralCache &operator=(CentralCache &copy) = delete;

private:
    SpanList _spanLists[FREE_LIST_NUM]; // 每个哈希桶中挂的是一个个Span
    static CentralCache _sInst;         // 饿汉模式创建一个CentralCache
};

#endif