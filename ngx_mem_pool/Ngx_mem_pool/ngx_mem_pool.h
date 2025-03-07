#pragma once
#include <cstring>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>

using u_char = unsigned char;
using ngx_uint_t = unsigned int;

struct ngx_pool_s;

//typedef void (*ngx_pool_cleanup_pt)(void* data);
//
//struct ngx_pool_cleanup_s {
//    ngx_pool_cleanup_pt   handler;//释放资源的回调函数
//    void* data;
//    ngx_pool_cleanup_s* next;//可能需要多个回调函数
//};

using ngx_pool_cleanup_handler = std::function<void()>;

struct ngx_pool_cleanup_s {
    ngx_pool_cleanup_handler   handler;//释放资源的回调函数
    ngx_pool_cleanup_s* next;//可能需要多个回调函数
};

//大块内存的头部信息
struct ngx_pool_large_s {
    ngx_pool_large_s* next;//指向下一个大块内存的头部信息
    void* alloc;//指向该头部对应的大块内存的起始地址
};


struct ngx_pool_data_s {
    u_char* last;//可用内存的起始地址
    u_char* end;//可用内存的终止地址
    ngx_pool_s* next;//指向下一个小块内存
    ngx_uint_t            failed;//记录内存分配失败的次数
};


struct ngx_pool_s {
    ngx_pool_data_s       d;//存储当前小块内存的使用情况
    size_t                max;//可以一次性分配的最大字节数
    ngx_pool_s* current;//指向当前可用的第一个小块内存的头部
    ngx_pool_large_s* large;//指向第一个大块内存的内存头
    ngx_pool_cleanup_s* cleanup;
};

//小块内存分配时对齐的单位
const int NGX_ALIGNMENT = sizeof(unsigned long);    /* platform word */
//将d对齐为a的倍数
#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

//清空buf缓冲区
#define ngx_memzero(buf, n)       (void) memset(buf, 0, n)
#define ngx_memset(buf, c, n)     (void) memset(buf, c, n)

//封装底层内存操作
#define ngx_free          free

//一个页面的大小
const int ngx_pagesize = 4096;
//允许从内存池中一次性分配的内存的最大值
const int NGX_MAX_ALLOC_FROM_POOL = ngx_pagesize - 1;
//默认的内存池开辟的大小
const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;
//内存池按16字节对齐
const int NGX_POOL_ALIGNMENT = 16;

//const int NGX_MIN_POOL_SIZE =
//        ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)), NGX_POOL_ALIGNMENT);


class Ngx_mem_pool
{
public:
    //参数size指定第一个内存块的大小
    Ngx_mem_pool(size_t size = NGX_DEFAULT_POOL_SIZE);
    ~Ngx_mem_pool();

    Ngx_mem_pool(const Ngx_mem_pool&) = delete;
    Ngx_mem_pool& operator=(const Ngx_mem_pool&) = delete;

    //从内存池申请size大小的内存，考虑内存字节对齐
    void* ngx_palloc(size_t size);
    //从内存池申请size大小的内存，不考虑内存字节对齐
    void* ngx_pnalloc(size_t size);
    //从内存池申请size大小的内存，内部调用ngx_palloc，然后将内存初始化为0
    void* ngx_pcalloc(size_t size);

    //内存重置函数
    void ngx_reset_pool();

    //向指定内存池中添加一个资源释放函数头部信息
    //参数size是资源释放函数需要的参数的大小
    //成功时返回头部信息的起始地址，失败返回NULL
    //ngx_pool_cleanup_s* ngx_pool_cleanup_add(size_t size);
    template<typename Func, typename... Args>
    bool ngx_pool_cleanup_add(Func&& f, Args&&... args)
    {
        auto handler = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);

		//创建一个释放资源的头部
        ngx_pool_cleanup_s* c;

		c = (ngx_pool_cleanup_s*)ngx_palloc(sizeof(ngx_pool_cleanup_s));
		if (c == nullptr) {
			return false;
		}

        //此处不能直接拷贝：c->handler = handler
        //因为分配的内存没有构造
        new(&(c->handler)) ngx_pool_cleanup_handler(handler);
		//将当前头部插入cleanup链表
		c->next = pool->cleanup;
		pool->cleanup = c;

		return true;
    }

private:
    //内存池的入口指针
    ngx_pool_s* pool;

    //从小块内存中获取大小为size的内存，align指定是否对齐
    void* ngx_palloc_small(size_t size, ngx_uint_t align);
    //分配一个大块内存，空闲内存大小由参数size指定
    void* ngx_palloc_large(size_t size);
    //释放指定的一个大块内存
    void ngx_pfree(void* p);

    //向内存池中添加小块内存，
    //并且分配size大小的内存，返回这段内存的起始地址
    void* ngx_palloc_block(size_t size);
    //申请指定大小的内存
    //成功返回首地址
    //失败返回nullptr
    void* ngx_alloc(size_t size);
};