#include "ngx_mem_pool.h"
#include <iostream>

Ngx_mem_pool::Ngx_mem_pool(size_t size): pool(nullptr)
{
    pool = (ngx_pool_s*)ngx_alloc(size);
    if (pool == nullptr) {
        std::cout << "Ngx_mem_pool init wrong" << std::endl;
        return;
    }
    //第一个内存块的头部信息为ngx_pool_s
    pool->d.last = (u_char*)pool + sizeof(ngx_pool_s);//空闲内存的起始地址为头部信息之后的地址
    pool->d.end = (u_char*)pool + size;//内存块的结束地址
    pool->d.next = nullptr;//指向下一个内存块
    pool->d.failed = 0;

    //获取当前内存块的空闲内存的大小
    size = size - sizeof(ngx_pool_s);
    //当前内存块可以一次性分配的内存大小
    pool->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

    pool->current = pool;
    pool->large = nullptr;
    pool->cleanup = nullptr;

    return;
}

Ngx_mem_pool::~Ngx_mem_pool()
{
    //首先遍历cleanup链表，释放内存池外部资源
    ngx_pool_cleanup_s* c;
    for (c = pool->cleanup; c; c = c->next) {
        if (c->handler) {
            //调用资源释放函数
            (c->handler)();
        }
    }

    //释放大块内存
    ngx_pool_large_s* l;
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }

    //释放小块内存
    ngx_pool_s* p, * n;
    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        ngx_free(p);

        if (n == nullptr) {
            break;
        }
    }
}

void* Ngx_mem_pool::ngx_palloc(size_t size)
{
    if (size <= pool->max) {//小内存块的分配
        return ngx_palloc_small(size, 1);
    }

    //大内存块的分配
    return ngx_palloc_large(size);
}

void* Ngx_mem_pool::ngx_pnalloc(size_t size)
{
    if (size <= pool->max) {
        return ngx_palloc_small(size, 0);
    }

    return ngx_palloc_large(size);
}

void* Ngx_mem_pool::ngx_pcalloc(size_t size)
{
    void* p;

    p = ngx_palloc(size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}

void Ngx_mem_pool::ngx_pfree(void* p)
{
    ngx_pool_large_s* l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            ngx_free(l->alloc);
            l->alloc = nullptr;

            return;
        }
    }

    return;
}

void Ngx_mem_pool::ngx_reset_pool()
{
    //源码没有在这里释放外部资源
    //这是因为虽然下面将大块内存释放了
    // 但资源释放函数仍然在cleanup链表上，在析构时自动释放
    //也就是说大块内存中的变量持有的外部资源并不是在该变量销毁时释放的

    ngx_pool_large_s* l;
    //遍历大块内存
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {//如果该头部有内存块，则释放
            ngx_free(l->alloc);
        }
    }

    //遍历小块内存
    //for (p = pool; p; p = p->d.next) {
    // //重置每个小块内存的空闲内存首地址指针
    // //这里有问题，只有第一块内存头部是ngx_pool_t，其他内存头部是ngx_pool_data_t。浪费空间
    //    p->d.last = (u_char*)p + sizeof(ngx_pool_s);
    //    p->d.failed = 0;
    //}

    //处理第一个小块内存
    ngx_pool_s* p = pool;
    p->d.last = (u_char*)p + sizeof(ngx_pool_s);
    p->d.last = 0;

    //从第二个小块内存开始循环，直到最后一个小块内存
    for (p = p->d.next; p; p = p->d.next)
    {
        p->d.last = (u_char*)p + sizeof(ngx_pool_data_s);
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->large = nullptr;
}

//ngx_pool_cleanup_s* Ngx_mem_pool::ngx_pool_cleanup_add(size_t size)
//{
//    //创建一个释放资源的头部
//    ngx_pool_cleanup_s* c;
//
//    c = (ngx_pool_cleanup_s*)ngx_palloc(sizeof(ngx_pool_cleanup_s));
//    if (c == nullptr) {
//        return nullptr;
//    }
//
//    //为资源释放函数的参数分配内存
//    if (size) {
//        c->data = ngx_palloc(size);
//        if (c->data == nullptr) {
//            return nullptr;
//        }
//
//    }
//    else {//不需要参数
//        c->data = nullptr;
//    }
//
//    //先将回调函数置为空
//    c->handler = nullptr;
//    //将当前头部插入cleanup链表
//    c->next = pool->cleanup;
//    pool->cleanup = c;
//
//    //ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, p->log, 0, "add cleanup: %p", c);
//
//    return c;
//}

void* Ngx_mem_pool::ngx_palloc_small(size_t size, ngx_uint_t align)
{
    u_char* m;
    ngx_pool_s* p;
    //获取当前内存池中正在使用的内存块
    p = pool->current;

    do {
        m = p->d.last;//当前内存块的空闲内存的起始地址

        if (align) {//对m进行对齐
            m = ngx_align_ptr(m, NGX_ALIGNMENT);
        }
        //如果当前空闲内存满足要求，则直接返回
        if ((size_t)(p->d.end - m) >= size) {
            p->d.last = m + size;

            return m;
        }
        //当前内存块的空闲内存不满足要求，查看下一个内存块
        p = p->d.next;

    } while (p);
    //当前所有内存块均不满足要求
    return ngx_palloc_block(size);
}

void* Ngx_mem_pool::ngx_palloc_large(size_t size)
{
    void* p;
    ngx_uint_t n;
    ngx_pool_large_s* large;

    //调用malloc分配内存
    p = ngx_alloc(size);
    if (p == nullptr) {
        return nullptr;
    }

    n = 0;
    //遍历所有大块内存
    //释放大块内存时只释放内存块不释放头部，申请大块内存时先查看有没有空闲的头部
    //如果找到一个空闲头部，则直接将刚才分配的内存交由该头部
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == nullptr) {
            large->alloc = p;
            return p;
        }
        //找了3个仍没有找到则放弃
        if (n++ > 3) {
            break;
        }
    }

    //从小块内存中获取空间放置大块内存的头部
    large = (ngx_pool_large_s*)ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
    if (large == nullptr) {
        ngx_free(p);
        return nullptr;
    }

    //连接头部与大块内存
    large->alloc = p;
    //将大块内存插入链表
    large->next = pool->large;
    pool->large = large;

    return p;
}

void* Ngx_mem_pool::ngx_palloc_block(size_t size)
{
    u_char* m;
    size_t psize;
    ngx_pool_s* p, * new_mem;

    //获取当前内存池一个小块内存块的大小
    psize = (size_t)(pool->d.end - (u_char*)pool);

    //分配内存
    m = (u_char*)ngx_alloc(psize);
    if (m == nullptr) {
        return nullptr;
    }

    //初始化头部信息
    new_mem = (ngx_pool_s*)m;

    new_mem->d.end = m + psize;
    new_mem->d.next = nullptr;
    new_mem->d.failed = 0;

    //获取空闲内存的首地址
    m += sizeof(ngx_pool_data_s);
    m = ngx_align_ptr(m, NGX_ALIGNMENT);
    new_mem->d.last = m + size;

    //遍历当前内存池的所有内存块，将它们头部的failed字段加一
    //并让current指向第一个failed字段不大于4的内存块
    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool->current = p->d.next;
        }
    }

    //将新的内存块添加到内存池中
    p->d.next = new_mem;

    return m;
}

void* Ngx_mem_pool::ngx_alloc(size_t size)
{
    void* p;

    p = malloc(size);
    if (p == NULL) {
        /*ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
            "malloc(%uz) failed", size);*/
        return nullptr;
    }

    return p;
}