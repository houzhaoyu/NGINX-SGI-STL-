#pragma once
#include <mutex>
#include <iostream>
/*
* 多线程 线程安全问题
* nginx可以在每个线程创建一个内存池，不需要考虑多线程
* 容器需要考虑
* 移植SGI STL二级空间配置器源码 模板实现
*/

template <int __inst>
class __malloc_alloc_template {

private:

	static void* _S_oom_malloc(size_t);
	static void* _S_oom_realloc(void*, size_t);
	static void (*__malloc_alloc_oom_handler)();//这是一个回调函数，由用户设置，用来释放内存资源


public:

	static void* allocate(size_t __n)
	{
		void* __result = malloc(__n);
		//如果分配空间失败，则调用_S_oom_malloc，oom：out of memory
		if (nullptr == __result) __result = _S_oom_malloc(__n);
		return __result;
	}

	static void deallocate(void* __p, size_t /* __n */)
	{
		free(__p);
	}

	static void* reallocate(void* __p, size_t /* old_sz */, size_t __new_sz)
	{
		void* __result = realloc(__p, __new_sz);
		if (0 == __result) __result = _S_oom_realloc(__p, __new_sz);
		return __result;
	}

	static void (*__set_malloc_handler(void (*__f)()))()
	{
		void (*__old)() = __malloc_alloc_oom_handler;
		__malloc_alloc_oom_handler = __f;
		return(__old);
	}

};

// malloc_alloc out-of-memory handling
template <int __inst>
void (*__malloc_alloc_template<__inst>::__malloc_alloc_oom_handler)() = nullptr;

template <int __inst>
void*
__malloc_alloc_template<__inst>::_S_oom_malloc(size_t __n)
{
	std::cout << "_S_oom_malloc" << std::endl;
	void (*__my_malloc_handler)();
	void* __result;
	//当用户设置了回调函数时，会一直循环调用回调函数然后分配内存，直到成功分配内存
		//因此，用户设置的回调函数必须拥有释放空间的功能，否则会陷入死循环
	for (;;) {
		__my_malloc_handler = __malloc_alloc_oom_handler;
		if (nullptr == __my_malloc_handler) { throw std::bad_alloc(); }//如果没有设置回调函数，则抛出异常
		(*__my_malloc_handler)();//否则，调用回调函数
		__result = malloc(__n);//再次尝试分配空间
		if (__result) return(__result);
	}
}

template <int __inst>
void* __malloc_alloc_template<__inst>::_S_oom_realloc(void* __p, size_t __n)
{
	std::cout << "_S_oom_realloc" << std::endl;
	void (*__my_malloc_handler)();
	void* __result;

	for (;;) {
		__my_malloc_handler = __malloc_alloc_oom_handler;
		if (0 == __my_malloc_handler) { throw std::bad_alloc(); }
		(*__my_malloc_handler)();
		__result = realloc(__p, __n);
		if (__result) return(__result);
	}
}

typedef __malloc_alloc_template<0> malloc_alloc;

template<typename T>
class myallocator
{
public:
	//以下代码必须存在，否则无法用于容器
	using value_type = T;
	constexpr myallocator() noexcept
	{
	}

	constexpr myallocator(const myallocator&) noexcept = default;
	template<class _Other>
	constexpr myallocator(const myallocator<_Other>&) noexcept
	{
	}
	//以上代码必须存在，否则无法用于容器
	
	//开辟内存
	//获取一个指定大小的chunk块，返回值为chunk块的地址
	//__n表示元素的个数
	T* allocate(size_t __n)
	{
		//std stl传入的是元素个数
		//这里需要的是空间大小
		__n = __n * sizeof(T);

		void* __ret = 0;

		if (__n > (size_t)_MAX_BYTES) {
			//当申请的内存块大于128字节则不受内存池管理
			__ret = malloc_alloc::allocate(__n);
		}
		else {
			//获取需要的内存块在自由链表中的编号
			_Obj* volatile* __my_free_list
				= _S_free_list + _S_freelist_index(__n);

			std::lock_guard<std::mutex> guard(mtx);
			//__result是数组中的一个元素，也就是链表的首节点
			_Obj*  __result = *__my_free_list;
			if (__result == nullptr)//如果没有对应大小的内存块，则分配
				__ret = _S_refill(_S_round_up(__n));
			else {
				//将链表中的第二个元素放在数组中，作为新的首节点
				*__my_free_list = __result->_M_free_list_link;
				//将第一个元素返回
				__ret = __result;
			}
		}

		return (T*)__ret;
	}
	//释放内存，__n表示元素的个数
	void deallocate(void* __p, size_t __n)
	{
		__n = __n * sizeof(T);
		//如果块大小大于128，那么它是通过malloc分配，就通过free释放
		if (__n > (size_t)_MAX_BYTES)
			malloc_alloc::deallocate(__p, __n);
		else {
			//获取要归还的链表的首节点
			_Obj* volatile* __my_free_list
				= _S_free_list + _S_freelist_index(__n);
			_Obj* __q = (_Obj*)__p;

			//操作链表时先加锁
			std::lock_guard<std::mutex> guard(mtx);
			//将它插入链表的头部
			__q->_M_free_list_link = *__my_free_list;
			*__my_free_list = __q;
			// lock is released here
		}
	}
	//内存扩容、缩容
	void* reallocate(void* __p, size_t __old_sz, size_t __new_sz)
	{
		void* __result;
		size_t __copy_sz;

		if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES) {
			return(realloc(__p, __new_sz));
		}
		//
		if (_S_round_up(__old_sz) == _S_round_up(__new_sz)) return(__p);
		__result = allocate(__new_sz);
		//将数据从老内存块拷贝到新的内存块，要考虑新老的大小问题
		__copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
		memcpy(__result, __p, __copy_sz);
		deallocate(__p, __old_sz);
		return(__result);
	}

	//对象构造
	void construct(T* _p, const T& val)
	{
		new(_p) T(val);
	}
	//对象析构
	void destroy(T* _p)
	{
		_p->~T();
	}

	void output_pool()
	{
		std::lock_guard<std::mutex> guard(mtx);
		std::cout << typeid(T).name() << std::endl;
		int size_per_list[_NFREELISTS];
		for (int i = 0; i < _NFREELISTS; ++i)
		{
			std::cout << i << " : ";
			int sum = 0;
			_Obj* current = *(_S_free_list + i);
			for (current; current != nullptr; current = current->_M_free_list_link)
			{
				std::cout << "-->" << (i + 1) * _ALIGN;
				sum += (i + 1) * _ALIGN;
			}
			size_per_list[i] = sum;
			std::cout << std::endl;
		}
		std::cout << std::endl;
		int sum = 0;
		for (int i = 0; i < _NFREELISTS; ++i)
		{
			sum += size_per_list[i];
			std::cout << i << " : " << size_per_list[i] << "bytes" << std::endl;
		}
		std::cout << "total size = " << sum << "bytes" << std::endl;
	}

private:
	enum { _ALIGN = 8 };//8字节对齐
	enum { _MAX_BYTES = 128 };//最大128字节
	enum { _NFREELISTS = 16 }; // _MAX_BYTES/_ALIGN，有16个链表

	//每个chunk块的头信息
	union _Obj {
		union _Obj* _M_free_list_link;//相当于next指针
		char _M_client_data[1];    /* The client sees this.        */
	};
	//表示存储自由链表的起始地址
	static _Obj* volatile _S_free_list[_NFREELISTS];

	static std::mutex mtx;

	static char* _S_start_free;//起始内存
	static char* _S_end_free;//终止内存
	static size_t _S_heap_size;//

	//将__bytes上调至最邻近的8的倍数
	static size_t _S_round_up(size_t __bytes)
	{
		return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
	}

	//返回__bytes大小的块位于自由链表中的编号
	static  size_t _S_freelist_index(size_t __bytes) {
		return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
	}

	//输入需要的内存块的大小__n，返回值为内存块的首地址。
	//一次性申请多个chunk块，返回第一个chunk块的地址，其他chunk块添加到链表中备用
	static void* _S_refill(size_t __n)
	{
		int __nobjs = 20;
		//开辟内存，并返回首地址 
		char* __chunk = _S_chunk_alloc(__n, __nobjs);
		_Obj* volatile* __my_free_list;
		_Obj* __result;
		_Obj* __current_obj;
		_Obj* __next_obj;
		int __i;

		//如果只返回了一个内存块大小的内存
		if (1 == __nobjs) return(__chunk);
		__my_free_list = _S_free_list + _S_freelist_index(__n);

		/* Build free list in chunk */
		__result = (_Obj*)__chunk;
		*__my_free_list = __next_obj = (_Obj*)(__chunk + __n);
		for (__i = 1; ; __i++) {
			__current_obj = __next_obj;
			__next_obj = (_Obj*)((char*)__next_obj + __n);
			if (__nobjs - 1 == __i) {
				__current_obj->_M_free_list_link = nullptr;
				break;
			}
			else {
				__current_obj->_M_free_list_link = __next_obj;
			}
		}
		return(__result);
	}

	//输入内存块的大小和数量，返回一定量的对应大小的内存块。
	// __size*__nobjs只是参考值，具体的数量可以通过读取__nobjs对应的实参的值获取。这些内存块是连续的，未被划分的
	static char* _S_chunk_alloc(size_t __size, int& __nobjs)
	{
		char* __result;
		size_t __total_bytes = __size * __nobjs;//需要分配的总内存大小
		size_t __bytes_left = _S_end_free - _S_start_free;//当前已经开辟的空闲内存数量

		if (__bytes_left >= __total_bytes) {
			//如果当前空闲内存满足要求
			__result = _S_start_free;
			_S_start_free += __total_bytes;
			return(__result);
		}
		else if (__bytes_left >= __size) {
			//如果当前空闲内存不满足总要求，但是大于一个内存块
			//那么就不再开辟内存，而是将现有的内存划分出来，返回整数个内存块大小的内存，并且将__nobjs设置成返回的内存块数量
			__nobjs = (int)(__bytes_left / __size);//计算空闲内存可以划分成几个内存块
			__total_bytes = __size * __nobjs;
			__result = _S_start_free;
			_S_start_free += __total_bytes;
			return(__result);
		}
		else {
			//如果当前剩余内存数量小于一个内存块的大小
			//_S_heap_size记录了以及分配了多大的内存
			//这样可以在__total_bytes不变的情况下，每次分配越来越多的空间，
			//因为分配的次数越多，说明消耗速度越快，所以需要每次分配更大的空间
			size_t __bytes_to_get =
				2 * __total_bytes + _S_round_up(_S_heap_size >> 4);
			// Try to make use of the left-over piece.
			if (__bytes_left > 0) {
				//将剩余的内存块放在其他大小的链表中（见下图）
				_Obj* volatile* __my_free_list =
					_S_free_list + _S_freelist_index(__bytes_left);

				((_Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
				*__my_free_list = (_Obj*)_S_start_free;
			}
			//将原来的空闲内存放到链表中，然后开辟新内存
			_S_start_free = (char*)malloc(__bytes_to_get);
			//如果分配失败
			if (nullptr == _S_start_free) {
				size_t __i;
				_Obj* volatile* __my_free_list;
				_Obj* __p;
				// Try to make do with what we have.  That can't
				// hurt.  We do not try smaller requests, since that tends
				// to result in disaster on multi-process machines.
				//从指定大小的chunk块大小size开始，每次加8，直到等于128
				//遍历所有大于size的chunk块链表，如果找到了chunk块，则将该块从链表中取出，放到空闲内存中
				for (__i = __size;
					__i <= (size_t)_MAX_BYTES;
					__i += (size_t)_ALIGN) 
				{
					__my_free_list = _S_free_list + _S_freelist_index(__i);
					__p = *__my_free_list;
					if (nullptr != __p) {
						*__my_free_list = __p->_M_free_list_link;
						_S_start_free = (char*)__p;
						_S_end_free = _S_start_free + __i;
						return(_S_chunk_alloc(__size, __nobjs));
						// Any leftover piece will eventually make it to the
						// right free list.
					}
				}
				//如果找不到任何chunk块
				_S_end_free = nullptr;	// In case of exception.
				_S_start_free = (char*)malloc_alloc::allocate(__bytes_to_get);
				// This should either throw an
				// exception or remedy the situation.  Thus we assume it
				// succeeded.
			}
			//如果分配成功
			_S_heap_size += __bytes_to_get;
			_S_end_free = _S_start_free + __bytes_to_get;
			return(_S_chunk_alloc(__size, __nobjs));//递归调用时肯定满足__bytes_left >= __total_bytes
		}
	}
};

template <typename T>
char* myallocator<T>::_S_start_free = nullptr;

template <typename T>
char* myallocator<T>::_S_end_free = nullptr;

template <typename T>
size_t myallocator<T>::_S_heap_size = 0;

template <typename T>
typename myallocator<T>::_Obj* volatile myallocator<T>::_S_free_list[_NFREELISTS] =
{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, };

template <typename T>
std::mutex myallocator<T>::mtx;