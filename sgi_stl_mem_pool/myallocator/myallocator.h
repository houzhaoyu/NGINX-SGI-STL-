#pragma once
#include <mutex>
#include <iostream>
/*
* ���߳� �̰߳�ȫ����
* nginx������ÿ���̴߳���һ���ڴ�أ�����Ҫ���Ƕ��߳�
* ������Ҫ����
* ��ֲSGI STL�����ռ�������Դ�� ģ��ʵ��
*/

template <int __inst>
class __malloc_alloc_template {

private:

	static void* _S_oom_malloc(size_t);
	static void* _S_oom_realloc(void*, size_t);
	static void (*__malloc_alloc_oom_handler)();//����һ���ص����������û����ã������ͷ��ڴ���Դ


public:

	static void* allocate(size_t __n)
	{
		void* __result = malloc(__n);
		//�������ռ�ʧ�ܣ������_S_oom_malloc��oom��out of memory
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
	//���û������˻ص�����ʱ����һֱѭ�����ûص�����Ȼ������ڴ棬ֱ���ɹ������ڴ�
		//��ˣ��û����õĻص���������ӵ���ͷſռ�Ĺ��ܣ������������ѭ��
	for (;;) {
		__my_malloc_handler = __malloc_alloc_oom_handler;
		if (nullptr == __my_malloc_handler) { throw std::bad_alloc(); }//���û�����ûص����������׳��쳣
		(*__my_malloc_handler)();//���򣬵��ûص�����
		__result = malloc(__n);//�ٴγ��Է���ռ�
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
	//���´��������ڣ������޷���������
	using value_type = T;
	constexpr myallocator() noexcept
	{
	}

	constexpr myallocator(const myallocator&) noexcept = default;
	template<class _Other>
	constexpr myallocator(const myallocator<_Other>&) noexcept
	{
	}
	//���ϴ��������ڣ������޷���������
	
	//�����ڴ�
	//��ȡһ��ָ����С��chunk�飬����ֵΪchunk��ĵ�ַ
	//__n��ʾԪ�صĸ���
	T* allocate(size_t __n)
	{
		//std stl�������Ԫ�ظ���
		//������Ҫ���ǿռ��С
		__n = __n * sizeof(T);

		void* __ret = 0;

		if (__n > (size_t)_MAX_BYTES) {
			//��������ڴ�����128�ֽ������ڴ�ع���
			__ret = malloc_alloc::allocate(__n);
		}
		else {
			//��ȡ��Ҫ���ڴ�������������еı��
			_Obj* volatile* __my_free_list
				= _S_free_list + _S_freelist_index(__n);

			std::lock_guard<std::mutex> guard(mtx);
			//__result�������е�һ��Ԫ�أ�Ҳ����������׽ڵ�
			_Obj*  __result = *__my_free_list;
			if (__result == nullptr)//���û�ж�Ӧ��С���ڴ�飬�����
				__ret = _S_refill(_S_round_up(__n));
			else {
				//�������еĵڶ���Ԫ�ط��������У���Ϊ�µ��׽ڵ�
				*__my_free_list = __result->_M_free_list_link;
				//����һ��Ԫ�ط���
				__ret = __result;
			}
		}

		return (T*)__ret;
	}
	//�ͷ��ڴ棬__n��ʾԪ�صĸ���
	void deallocate(void* __p, size_t __n)
	{
		__n = __n * sizeof(T);
		//������С����128����ô����ͨ��malloc���䣬��ͨ��free�ͷ�
		if (__n > (size_t)_MAX_BYTES)
			malloc_alloc::deallocate(__p, __n);
		else {
			//��ȡҪ�黹��������׽ڵ�
			_Obj* volatile* __my_free_list
				= _S_free_list + _S_freelist_index(__n);
			_Obj* __q = (_Obj*)__p;

			//��������ʱ�ȼ���
			std::lock_guard<std::mutex> guard(mtx);
			//�������������ͷ��
			__q->_M_free_list_link = *__my_free_list;
			*__my_free_list = __q;
			// lock is released here
		}
	}
	//�ڴ����ݡ�����
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
		//�����ݴ����ڴ�鿽�����µ��ڴ�飬Ҫ�������ϵĴ�С����
		__copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
		memcpy(__result, __p, __copy_sz);
		deallocate(__p, __old_sz);
		return(__result);
	}

	//������
	void construct(T* _p, const T& val)
	{
		new(_p) T(val);
	}
	//��������
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
	enum { _ALIGN = 8 };//8�ֽڶ���
	enum { _MAX_BYTES = 128 };//���128�ֽ�
	enum { _NFREELISTS = 16 }; // _MAX_BYTES/_ALIGN����16������

	//ÿ��chunk���ͷ��Ϣ
	union _Obj {
		union _Obj* _M_free_list_link;//�൱��nextָ��
		char _M_client_data[1];    /* The client sees this.        */
	};
	//��ʾ�洢�����������ʼ��ַ
	static _Obj* volatile _S_free_list[_NFREELISTS];

	static std::mutex mtx;

	static char* _S_start_free;//��ʼ�ڴ�
	static char* _S_end_free;//��ֹ�ڴ�
	static size_t _S_heap_size;//

	//��__bytes�ϵ������ڽ���8�ı���
	static size_t _S_round_up(size_t __bytes)
	{
		return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
	}

	//����__bytes��С�Ŀ�λ�����������еı��
	static  size_t _S_freelist_index(size_t __bytes) {
		return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
	}

	//������Ҫ���ڴ��Ĵ�С__n������ֵΪ�ڴ����׵�ַ��
	//һ����������chunk�飬���ص�һ��chunk��ĵ�ַ������chunk����ӵ������б���
	static void* _S_refill(size_t __n)
	{
		int __nobjs = 20;
		//�����ڴ棬�������׵�ַ 
		char* __chunk = _S_chunk_alloc(__n, __nobjs);
		_Obj* volatile* __my_free_list;
		_Obj* __result;
		_Obj* __current_obj;
		_Obj* __next_obj;
		int __i;

		//���ֻ������һ���ڴ���С���ڴ�
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

	//�����ڴ��Ĵ�С������������һ�����Ķ�Ӧ��С���ڴ�顣
	// __size*__nobjsֻ�ǲο�ֵ���������������ͨ����ȡ__nobjs��Ӧ��ʵ�ε�ֵ��ȡ����Щ�ڴ���������ģ�δ�����ֵ�
	static char* _S_chunk_alloc(size_t __size, int& __nobjs)
	{
		char* __result;
		size_t __total_bytes = __size * __nobjs;//��Ҫ��������ڴ��С
		size_t __bytes_left = _S_end_free - _S_start_free;//��ǰ�Ѿ����ٵĿ����ڴ�����

		if (__bytes_left >= __total_bytes) {
			//�����ǰ�����ڴ�����Ҫ��
			__result = _S_start_free;
			_S_start_free += __total_bytes;
			return(__result);
		}
		else if (__bytes_left >= __size) {
			//�����ǰ�����ڴ治������Ҫ�󣬵��Ǵ���һ���ڴ��
			//��ô�Ͳ��ٿ����ڴ棬���ǽ����е��ڴ滮�ֳ����������������ڴ���С���ڴ棬���ҽ�__nobjs���óɷ��ص��ڴ������
			__nobjs = (int)(__bytes_left / __size);//��������ڴ���Ի��ֳɼ����ڴ��
			__total_bytes = __size * __nobjs;
			__result = _S_start_free;
			_S_start_free += __total_bytes;
			return(__result);
		}
		else {
			//�����ǰʣ���ڴ�����С��һ���ڴ��Ĵ�С
			//_S_heap_size��¼���Լ������˶����ڴ�
			//����������__total_bytes���������£�ÿ�η���Խ��Խ��Ŀռ䣬
			//��Ϊ����Ĵ���Խ�࣬˵�������ٶ�Խ�죬������Ҫÿ�η������Ŀռ�
			size_t __bytes_to_get =
				2 * __total_bytes + _S_round_up(_S_heap_size >> 4);
			// Try to make use of the left-over piece.
			if (__bytes_left > 0) {
				//��ʣ����ڴ�����������С�������У�����ͼ��
				_Obj* volatile* __my_free_list =
					_S_free_list + _S_freelist_index(__bytes_left);

				((_Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
				*__my_free_list = (_Obj*)_S_start_free;
			}
			//��ԭ���Ŀ����ڴ�ŵ������У�Ȼ�󿪱����ڴ�
			_S_start_free = (char*)malloc(__bytes_to_get);
			//�������ʧ��
			if (nullptr == _S_start_free) {
				size_t __i;
				_Obj* volatile* __my_free_list;
				_Obj* __p;
				// Try to make do with what we have.  That can't
				// hurt.  We do not try smaller requests, since that tends
				// to result in disaster on multi-process machines.
				//��ָ����С��chunk���Сsize��ʼ��ÿ�μ�8��ֱ������128
				//�������д���size��chunk����������ҵ���chunk�飬�򽫸ÿ��������ȡ�����ŵ������ڴ���
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
				//����Ҳ����κ�chunk��
				_S_end_free = nullptr;	// In case of exception.
				_S_start_free = (char*)malloc_alloc::allocate(__bytes_to_get);
				// This should either throw an
				// exception or remedy the situation.  Thus we assume it
				// succeeded.
			}
			//�������ɹ�
			_S_heap_size += __bytes_to_get;
			_S_end_free = _S_start_free + __bytes_to_get;
			return(_S_chunk_alloc(__size, __nobjs));//�ݹ����ʱ�϶�����__bytes_left >= __total_bytes
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