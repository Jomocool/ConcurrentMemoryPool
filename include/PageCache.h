#ifndef PAGE_CACHE
#define PAGE_CACHE

#include "Common.h"
#include <unordered_map>

// 向PageCache申请Span，假设要申请4页
// 1.查看_spanLists[4]是否有空闲Span，有则分配，没有就下一步
// 2.向更大的页数对应的自由链表申请，有就把Span分成两块，没有就向系统申请（mmap/brk/VirtualAlloc）128页page Span，重复第一步
class PageCache
{
public:
    static PageCache *GetInstance()
    {
        return &_sInst;
    }

    // pc中_spanLists从取出一个管理着k页的Span
    Span *NewSpan(size_t k);
    // 通过页地址找到Span
    Span *MapObjectToSpan(void *obj);
    // 管理cc归还回来的span
    void ReleaseSpanToPageCache(Span *span);

private:
    SpanList _spanLists[PAGE_NUM + 1]; // PageCache中的哈希自由链表，以页为单位管理，下标代表一个Span管理的页数

public:
    std::mutex _pageMtx;                           // PageCache整体锁，这里不用桶锁是因为tc向PageCache申请Span时，可能会涉及多个桶
    std::unordered_map<PageID, Span *> _idSpanMap; // 页号到Span的快速映射表，只有分配出去的Span才需要记录

private:
    PageCache() {};
    PageCache(PageCache &copy) = delete;
    PageCache &operator=(PageCache &copy) = delete;

    static PageCache _sInst; // 单例PageCache对象
};

#endif