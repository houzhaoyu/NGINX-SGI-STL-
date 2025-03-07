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
//    ngx_pool_cleanup_pt   handler;//�ͷ���Դ�Ļص�����
//    void* data;
//    ngx_pool_cleanup_s* next;//������Ҫ����ص�����
//};

using ngx_pool_cleanup_handler = std::function<void()>;

struct ngx_pool_cleanup_s {
    ngx_pool_cleanup_handler   handler;//�ͷ���Դ�Ļص�����
    ngx_pool_cleanup_s* next;//������Ҫ����ص�����
};

//����ڴ��ͷ����Ϣ
struct ngx_pool_large_s {
    ngx_pool_large_s* next;//ָ����һ������ڴ��ͷ����Ϣ
    void* alloc;//ָ���ͷ����Ӧ�Ĵ���ڴ����ʼ��ַ
};


struct ngx_pool_data_s {
    u_char* last;//�����ڴ����ʼ��ַ
    u_char* end;//�����ڴ����ֹ��ַ
    ngx_pool_s* next;//ָ����һ��С���ڴ�
    ngx_uint_t            failed;//��¼�ڴ����ʧ�ܵĴ���
};


struct ngx_pool_s {
    ngx_pool_data_s       d;//�洢��ǰС���ڴ��ʹ�����
    size_t                max;//����һ���Է��������ֽ���
    ngx_pool_s* current;//ָ��ǰ���õĵ�һ��С���ڴ��ͷ��
    ngx_pool_large_s* large;//ָ���һ������ڴ���ڴ�ͷ
    ngx_pool_cleanup_s* cleanup;
};

//С���ڴ����ʱ����ĵ�λ
const int NGX_ALIGNMENT = sizeof(unsigned long);    /* platform word */
//��d����Ϊa�ı���
#define ngx_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define ngx_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

//���buf������
#define ngx_memzero(buf, n)       (void) memset(buf, 0, n)
#define ngx_memset(buf, c, n)     (void) memset(buf, c, n)

//��װ�ײ��ڴ����
#define ngx_free          free

//һ��ҳ��Ĵ�С
const int ngx_pagesize = 4096;
//������ڴ����һ���Է�����ڴ�����ֵ
const int NGX_MAX_ALLOC_FROM_POOL = ngx_pagesize - 1;
//Ĭ�ϵ��ڴ�ؿ��ٵĴ�С
const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;
//�ڴ�ذ�16�ֽڶ���
const int NGX_POOL_ALIGNMENT = 16;

//const int NGX_MIN_POOL_SIZE =
//        ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)), NGX_POOL_ALIGNMENT);


class Ngx_mem_pool
{
public:
    //����sizeָ����һ���ڴ��Ĵ�С
    Ngx_mem_pool(size_t size = NGX_DEFAULT_POOL_SIZE);
    ~Ngx_mem_pool();

    Ngx_mem_pool(const Ngx_mem_pool&) = delete;
    Ngx_mem_pool& operator=(const Ngx_mem_pool&) = delete;

    //���ڴ������size��С���ڴ棬�����ڴ��ֽڶ���
    void* ngx_palloc(size_t size);
    //���ڴ������size��С���ڴ棬�������ڴ��ֽڶ���
    void* ngx_pnalloc(size_t size);
    //���ڴ������size��С���ڴ棬�ڲ�����ngx_palloc��Ȼ���ڴ��ʼ��Ϊ0
    void* ngx_pcalloc(size_t size);

    //�ڴ����ú���
    void ngx_reset_pool();

    //��ָ���ڴ�������һ����Դ�ͷź���ͷ����Ϣ
    //����size����Դ�ͷź�����Ҫ�Ĳ����Ĵ�С
    //�ɹ�ʱ����ͷ����Ϣ����ʼ��ַ��ʧ�ܷ���NULL
    //ngx_pool_cleanup_s* ngx_pool_cleanup_add(size_t size);
    template<typename Func, typename... Args>
    bool ngx_pool_cleanup_add(Func&& f, Args&&... args)
    {
        auto handler = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);

		//����һ���ͷ���Դ��ͷ��
        ngx_pool_cleanup_s* c;

		c = (ngx_pool_cleanup_s*)ngx_palloc(sizeof(ngx_pool_cleanup_s));
		if (c == nullptr) {
			return false;
		}

        //�˴�����ֱ�ӿ�����c->handler = handler
        //��Ϊ������ڴ�û�й���
        new(&(c->handler)) ngx_pool_cleanup_handler(handler);
		//����ǰͷ������cleanup����
		c->next = pool->cleanup;
		pool->cleanup = c;

		return true;
    }

private:
    //�ڴ�ص����ָ��
    ngx_pool_s* pool;

    //��С���ڴ��л�ȡ��СΪsize���ڴ棬alignָ���Ƿ����
    void* ngx_palloc_small(size_t size, ngx_uint_t align);
    //����һ������ڴ棬�����ڴ��С�ɲ���sizeָ��
    void* ngx_palloc_large(size_t size);
    //�ͷ�ָ����һ������ڴ�
    void ngx_pfree(void* p);

    //���ڴ�������С���ڴ棬
    //���ҷ���size��С���ڴ棬��������ڴ����ʼ��ַ
    void* ngx_palloc_block(size_t size);
    //����ָ����С���ڴ�
    //�ɹ������׵�ַ
    //ʧ�ܷ���nullptr
    void* ngx_alloc(size_t size);
};