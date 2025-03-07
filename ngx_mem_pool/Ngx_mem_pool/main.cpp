#include <iostream>
#include "ngx_mem_pool.h"

typedef struct Data stData;
struct Data
{
    char* ptr;
    FILE* pfile;
};

int func(char* p, FILE* f)
{
    if (p) {
        free(p);
        p = nullptr;
    }
    if (f)
    {
        fclose(f);
        f = nullptr;
    }
    return 1;
}

//int main()
//{
//    ngx_pool_cleanup_s* c = (ngx_pool_cleanup_s*)malloc(sizeof(ngx_pool_cleanup_s));
//    if (nullptr == c)
//    {
//        std::cout << "c wrong" << std::endl;
//        return -1;
//    }
//    char* p = (char*)malloc(5);
//    if (nullptr == p)
//    {
//        std::cout << "p wrong" << std::endl;
//        return -1;
//    }
//    c->handler = std::bind(func1, p);
//    (c->handler)();
//    free(c);
//
//    return 0;
//}

int main()
{
    Ngx_mem_pool mem_pool(512);

    void* p1 = mem_pool.ngx_palloc(128); // ��С���ڴ�ط����
    if (nullptr == p1)
    {
        printf("ngx_palloc 128 bytes fail...");
        return -1;
    }

    //p2ָ��Ľṹ����ڴ���ڴ��У�ͬʱ�ڲ�����һЩ��Դ����Ҫ�����ͷ�
    stData* p2 = (stData*)mem_pool.ngx_palloc(512); // �Ӵ���ڴ�ط����
    if (nullptr == p2)
    {
        printf("ngx_palloc 512 bytes fail...");
        return -1;
    }
    p2->ptr = (char*)malloc(12);
    strcpy(p2->ptr, "hello world");
    p2->pfile = fopen("data.txt", "w");

    if (!mem_pool.ngx_pool_cleanup_add(func, p2->ptr, p2->pfile))
    {
        std::cout << "ngx_pool_cleanup_add(func1, p2->ptr) wrong" << std::endl;
    }
    mem_pool.ngx_reset_pool();

    //������Դ�ͷź���
    //ngx_pool_cleanup_s* c1 = mem_pool.ngx_pool_cleanup_add(sizeof(char*));
    //c1->handler = func1;
    //c1->data = p2->ptr;

    //ngx_pool_cleanup_s* c2 = mem_pool.ngx_pool_cleanup_add(sizeof(FILE*));
    //c2->handler = func2;
    //c2->data = p2->pfile;

    return 0;
}
