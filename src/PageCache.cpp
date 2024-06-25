#include "../include/PageCache.h"

PageCache PageCache::_sInst; // 饿汉模式下的单例

Span *PageCache::NewSpan(size_t k)
{
    // ① k号桶中有Span
    if (!_spanLists[k].empty())
    {
        Span *span = _spanLists[k].pop_front();

        // 记录分配出去的Span管理的页号和其地址的映射关系
        for (PageID i = 0; i < span->_n; ++i)
        {
            _idSpanMap[span->_pageID + i] = span;
        }

        return span;
    }
    // ② k号桶中没有Span，但是后面的桶有
    for (int i = k + 1; i <= PAGE_NUM; ++i)
    {
        if (!_spanLists[i].empty())
        {
            Span *nSpan = _spanLists[i].pop_front();

            // 分成一个k页Span和一个i-k页Span
            Span *kSpan = new Span;
            kSpan->_pageID = nSpan->_pageID;
            kSpan->_n = k;

            nSpan->_pageID += k;
            nSpan->_n -= k;

            _spanLists[nSpan->_n].push_front(nSpan);

            // 映射边缘页，方便回收Span时进行合并
            // 只需要映射被拆分后剩下的Span的边缘页，因为回收时待合并的Span都是被分配出去的
            _idSpanMap[nSpan->_pageID] = nSpan;
            _idSpanMap[nSpan->_pageID + nSpan->_n - 1] = nSpan;

            for (PageID i = 0; i < kSpan->_n; ++i)
            {
                _idSpanMap[kSpan->_pageID + i] = kSpan;
            }

            return kSpan;
        }
    }
    // ③ 都没有Span，向系统申请128页空间，然后再拆分
    void *ptr = SystemAlloc(PAGE_NUM);
    Span *bigSpan = new Span;
    bigSpan->_pageID = ((PageID)ptr) >> PAGE_SHIFT; // 系统分配的页面一定是对齐的
    bigSpan->_n = PAGE_NUM;
    _spanLists[bigSpan->_n].push_front(bigSpan);

    // 递归后必走②
    return NewSpan(k); // 复用代码
}

Span *PageCache::MapObjectToSpan(void *obj)
{
    // 找到页号
    PageID id = ((PageID)obj) >> PAGE_SHIFT;

    auto ret = _idSpanMap.find(id);

    if (ret == _idSpanMap.end())
    {
        assert(false);
        return nullptr;
    }

    return ret->second;
}

void PageCache::ReleaseSpanToPageCache(Span *span)
{
    // 向左不断合并
    while (1)
    {
        PageID leftID = span->_pageID - 1; // 左边相邻页id
        auto ret = _idSpanMap.find(leftID);
        // 没有相邻Span，停止合并
        if (ret == _idSpanMap.end())
            break;

        // 相邻Span在cc中，停止合并
        Span *leftSpan = ret->second;
        if (leftSpan->_isUse)
            break;

        // 相邻Span与当前Span合并后超过了128页，停止合并
        if (leftSpan->_n + span->_n > PAGE_NUM)
            break;

        // 当前Span与相邻Span合并
        span->_pageID = leftSpan->_pageID;
        span->_n += leftSpan->_n;

        _idSpanMap.erase(leftSpan->_pageID);
        _spanLists[leftSpan->_n].erase(leftSpan);
        delete leftSpan;
    }

    // 向右不断合并
    while (1)
    {
        PageID rightID = span->_pageID + span->_n;
        auto ret = _idSpanMap.find(rightID);

        // 没有相邻Span，停止合并
        if (ret == _idSpanMap.end())
            break;

        // 相邻Span在cc中，停止合并
        Span *rightSpan = ret->second;
        if (rightSpan->_isUse)
            break;

        // 相邻Span与当前Span合并后超过了128页，停止合并
        if (rightSpan->_n + span->_n > PAGE_NUM)
            break;

        // 当前Span与相邻Span合并
        span->_n += rightSpan->_n;

        _idSpanMap.erase(rightSpan->_pageID);
        _spanLists[rightSpan->_n].erase(rightSpan);
        delete rightSpan;
    }

    // 合并完毕，将当前Span挂到对应桶中
    _spanLists[span->_n].push_front(span);
    span->_isUse = false;

    // 映射边缘页，方便后续其它Span的合并
    _idSpanMap[span->_pageID] = span;
    _idSpanMap[span->_pageID + span->_n - 1] = span;
}