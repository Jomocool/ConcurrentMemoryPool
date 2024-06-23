#include "../include/PageCache.h"

PageCache PageCache::_sInst; // 饿汉模式下的单例

Span *PageCache::NewSpan(size_t k)
{
    // ① k号桶中有Span
    if (!_spanLists[k].empty())
        return _spanLists[k].pop_front();
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
