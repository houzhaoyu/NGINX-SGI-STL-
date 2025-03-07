#include "ngx_mem_pool.h"
#include <iostream>

Ngx_mem_pool::Ngx_mem_pool(size_t size): pool(nullptr)
{
    pool = (ngx_pool_s*)ngx_alloc(size);
    if (pool == nullptr) {
        std::cout << "Ngx_mem_pool init wrong" << std::endl;
        return;
    }
    //��һ���ڴ���ͷ����ϢΪngx_pool_s
    pool->d.last = (u_char*)pool + sizeof(ngx_pool_s);//�����ڴ����ʼ��ַΪͷ����Ϣ֮��ĵ�ַ
    pool->d.end = (u_char*)pool + size;//�ڴ��Ľ�����ַ
    pool->d.next = nullptr;//ָ����һ���ڴ��
    pool->d.failed = 0;

    //��ȡ��ǰ�ڴ��Ŀ����ڴ�Ĵ�С
    size = size - sizeof(ngx_pool_s);
    //��ǰ�ڴ�����һ���Է�����ڴ��С
    pool->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

    pool->current = pool;
    pool->large = nullptr;
    pool->cleanup = nullptr;

    return;
}

Ngx_mem_pool::~Ngx_mem_pool()
{
    //���ȱ���cleanup�����ͷ��ڴ���ⲿ��Դ
    ngx_pool_cleanup_s* c;
    for (c = pool->cleanup; c; c = c->next) {
        if (c->handler) {
            //������Դ�ͷź���
            (c->handler)();
        }
    }

    //�ͷŴ���ڴ�
    ngx_pool_large_s* l;
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }

    //�ͷ�С���ڴ�
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
    if (size <= pool->max) {//С�ڴ��ķ���
        return ngx_palloc_small(size, 1);
    }

    //���ڴ��ķ���
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
    //Դ��û���������ͷ��ⲿ��Դ
    //������Ϊ��Ȼ���潫����ڴ��ͷ���
    // ����Դ�ͷź�����Ȼ��cleanup�����ϣ�������ʱ�Զ��ͷ�
    //Ҳ����˵����ڴ��еı������е��ⲿ��Դ�������ڸñ�������ʱ�ͷŵ�

    ngx_pool_large_s* l;
    //��������ڴ�
    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {//�����ͷ�����ڴ�飬���ͷ�
            ngx_free(l->alloc);
        }
    }

    //����С���ڴ�
    //for (p = pool; p; p = p->d.next) {
    // //����ÿ��С���ڴ�Ŀ����ڴ��׵�ַָ��
    // //���������⣬ֻ�е�һ���ڴ�ͷ����ngx_pool_t�������ڴ�ͷ����ngx_pool_data_t���˷ѿռ�
    //    p->d.last = (u_char*)p + sizeof(ngx_pool_s);
    //    p->d.failed = 0;
    //}

    //�����һ��С���ڴ�
    ngx_pool_s* p = pool;
    p->d.last = (u_char*)p + sizeof(ngx_pool_s);
    p->d.last = 0;

    //�ӵڶ���С���ڴ濪ʼѭ����ֱ�����һ��С���ڴ�
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
//    //����һ���ͷ���Դ��ͷ��
//    ngx_pool_cleanup_s* c;
//
//    c = (ngx_pool_cleanup_s*)ngx_palloc(sizeof(ngx_pool_cleanup_s));
//    if (c == nullptr) {
//        return nullptr;
//    }
//
//    //Ϊ��Դ�ͷź����Ĳ��������ڴ�
//    if (size) {
//        c->data = ngx_palloc(size);
//        if (c->data == nullptr) {
//            return nullptr;
//        }
//
//    }
//    else {//����Ҫ����
//        c->data = nullptr;
//    }
//
//    //�Ƚ��ص�������Ϊ��
//    c->handler = nullptr;
//    //����ǰͷ������cleanup����
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
    //��ȡ��ǰ�ڴ��������ʹ�õ��ڴ��
    p = pool->current;

    do {
        m = p->d.last;//��ǰ�ڴ��Ŀ����ڴ����ʼ��ַ

        if (align) {//��m���ж���
            m = ngx_align_ptr(m, NGX_ALIGNMENT);
        }
        //�����ǰ�����ڴ�����Ҫ����ֱ�ӷ���
        if ((size_t)(p->d.end - m) >= size) {
            p->d.last = m + size;

            return m;
        }
        //��ǰ�ڴ��Ŀ����ڴ治����Ҫ�󣬲鿴��һ���ڴ��
        p = p->d.next;

    } while (p);
    //��ǰ�����ڴ���������Ҫ��
    return ngx_palloc_block(size);
}

void* Ngx_mem_pool::ngx_palloc_large(size_t size)
{
    void* p;
    ngx_uint_t n;
    ngx_pool_large_s* large;

    //����malloc�����ڴ�
    p = ngx_alloc(size);
    if (p == nullptr) {
        return nullptr;
    }

    n = 0;
    //�������д���ڴ�
    //�ͷŴ���ڴ�ʱֻ�ͷ��ڴ�鲻�ͷ�ͷ�����������ڴ�ʱ�Ȳ鿴��û�п��е�ͷ��
    //����ҵ�һ������ͷ������ֱ�ӽ��ղŷ�����ڴ潻�ɸ�ͷ��
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == nullptr) {
            large->alloc = p;
            return p;
        }
        //����3����û���ҵ������
        if (n++ > 3) {
            break;
        }
    }

    //��С���ڴ��л�ȡ�ռ���ô���ڴ��ͷ��
    large = (ngx_pool_large_s*)ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
    if (large == nullptr) {
        ngx_free(p);
        return nullptr;
    }

    //����ͷ�������ڴ�
    large->alloc = p;
    //������ڴ��������
    large->next = pool->large;
    pool->large = large;

    return p;
}

void* Ngx_mem_pool::ngx_palloc_block(size_t size)
{
    u_char* m;
    size_t psize;
    ngx_pool_s* p, * new_mem;

    //��ȡ��ǰ�ڴ��һ��С���ڴ��Ĵ�С
    psize = (size_t)(pool->d.end - (u_char*)pool);

    //�����ڴ�
    m = (u_char*)ngx_alloc(psize);
    if (m == nullptr) {
        return nullptr;
    }

    //��ʼ��ͷ����Ϣ
    new_mem = (ngx_pool_s*)m;

    new_mem->d.end = m + psize;
    new_mem->d.next = nullptr;
    new_mem->d.failed = 0;

    //��ȡ�����ڴ���׵�ַ
    m += sizeof(ngx_pool_data_s);
    m = ngx_align_ptr(m, NGX_ALIGNMENT);
    new_mem->d.last = m + size;

    //������ǰ�ڴ�ص������ڴ�飬������ͷ����failed�ֶμ�һ
    //����currentָ���һ��failed�ֶβ�����4���ڴ��
    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool->current = p->d.next;
        }
    }

    //���µ��ڴ����ӵ��ڴ����
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