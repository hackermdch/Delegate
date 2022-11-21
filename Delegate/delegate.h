#pragma once
#if !((__cplusplus >= 201703L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201703L) && (_MSC_VER >= 1913)))
#error "Please use C++17 or above"
#endif

#ifndef _WIN64
#error "Only windows 64 bit platform is supported"
#endif

#include <Windows.h>
#include <functional>

class MemPage final
{
	using byte = unsigned char;
private:
	static constexpr size_t MaxSize = 10000;
	void* ptr = nullptr;
	static constexpr size_t Size = (size_t)4 * 1024 * 1024;
	size_t used;
	CRITICAL_SECTION lock;
public:
	int refCount = 0;
	static MemPage* CurrentPage;
public:
	MemPage(const MemPage&) = delete;
	MemPage(MemPage&&) = delete;
	void operator=(const MemPage&) = delete;
	void operator=(MemPage&&) = delete;

	static size_t GetFuncSize(void* func)
	{
		size_t s = 0;
		byte* ptr = (byte*)func;
		while (s < MaxSize)
		{
			if (ptr[0] == 0x5f && ptr[1] == 0x5d && ptr[2] == 0xc3)
			{
				s += 3;
				break;
			}
			if (ptr[0] == 0xcc)
			{
				break;
			}
			s++;
			ptr++;
		}
		return s;
	}

	void* GetBase()
	{
		return ptr;
	}

	size_t GetUsed()
	{
		return used;
	}

	explicit MemPage() : used(0)
	{
		if (!InitializeCriticalSectionAndSpinCount(&lock, 10)) throw std::exception("can't initialize critical section");
		ptr = VirtualAlloc(nullptr, Size, MEM_COMMIT, PAGE_READWRITE);
		if (ptr == nullptr) throw std::exception("can't alloc virtual memory");
		memset(ptr, 0xcc, Size);
	}

	void Use(size_t size)
	{
		used += size;
	}

	bool test(size_t size)
	{
		return used + size < Size;
	}

	void Close()
	{
		DWORD _;
		if (!VirtualProtect(ptr, Size, PAGE_EXECUTE_READ, &_)) throw std::exception("can't close memory page");
	}

	void Open()
	{
		DWORD _;
		if (!VirtualProtect(ptr, Size, PAGE_READWRITE, &_)) throw std::exception("can't open memory page");
	}

	void Lock()
	{
		EnterCriticalSection(&lock);
	}

	void Unlock()
	{
		LeaveCriticalSection(&lock);
	}

	~MemPage()
	{
		DeleteCriticalSection(&lock);
		if (!VirtualFree(ptr, 0, MEM_RELEASE)) throw std::exception("can't release virtual memory");
		ptr = nullptr;
		used = 0;
	}
};

class MemFunc final
{
private:
	MemPage* page;
	void* ptr;
	size_t size;
public:
	MemFunc(const MemFunc&) = delete;
	MemFunc(MemFunc&&) = delete;
	void operator=(const MemFunc&) = delete;
	void operator=(MemFunc&&) = delete;

	MemFunc(MemPage* page, size_t size) : page(page), size(size)
	{
		ptr = (char*)page->GetBase() + page->GetUsed();
		page->refCount++;
		MemPage::CurrentPage->Use(size + 16);
	}

	~MemFunc()
	{
		page->refCount--;
		if (page->refCount == 0)
		{
			delete page;
			if (page == MemPage::CurrentPage) MemPage::CurrentPage = nullptr;
		}
		ptr = nullptr;
		size = 0;
	}

	void* GetPtr()
	{
		return ptr;
	}

	size_t GetSize()
	{
		return size;
	}
};

template <typename Ret, typename ...Args>
class Delegate final
{
	using byte = unsigned char;
	using Func = Ret(*)(Args...);
private:
	class InvokeAble
	{
	public:
		virtual Ret Invoke(Args... args) = 0;
	};

	class Function final : public InvokeAble
	{
	private:
		std::function<Ret(Args...)> function;
	public:
		Function() = delete;

		explicit Function(std::function<Ret(Args ...)> func) : function(std::move(func))
		{
		}

		Ret Invoke(Args... args) override
		{
			return function(std::forward<Args>(args)...);
		}
	};

	template<typename Cls>
	class Method final : public InvokeAble
	{
	private:
		Cls& object;
		union
		{
			Ret(Cls::* method)(Args...);
			void* ptr;
		};
	public:
		Method() = delete;

		Method(Cls& obj, Ret(Cls::* func)(Args...)) : object(obj), method(func)
		{
		}

		Method(Cls& obj, void* func) : object(obj), ptr(func)
		{
		}

		Ret Invoke(Args... args) override
		{
			return (object.*method)(std::forward<Args>(args)...);
		}
	};
private:
	InvokeAble* invoker;
	MemFunc* func = nullptr;

	static Ret forward(Args... args)
	{
		return ((Delegate<Ret, Args...>*)0x7fffffffffff)->invoker->Invoke(args...);
	}

private:
	void Init()
	{
#ifdef NDEBUG
		char* address = (char*)forward;
#else
		auto cp = (char*)forward;
		char* address = nullptr;
		memcpy(&address, cp + 1, 4);
		address = (char*)((long long)address + (long long)cp + 5);
#endif
		const auto size = MemPage::GetFuncSize(address);
		if (MemPage::CurrentPage == nullptr || !MemPage::CurrentPage->test(size)) MemPage::CurrentPage = new MemPage();
		MemPage::CurrentPage->Lock();
		try {
			MemPage::CurrentPage->Open();
			if (func == nullptr) func = new MemFunc(MemPage::CurrentPage, size + 16);
			byte* ptr = (byte*)func->GetPtr();
			memcpy(ptr, address, size);
			for (uint64_t i = 0; i < size; i++, ++ptr)
			{
				if (ptr[0] == 0x7f)
				{
					uint64_t l = 0;
					memcpy(&l, ptr - 5, 6);
					if (l == 0x7fffffffffff)
					{
						ptr -= 5;
						void* a = this;
						memcpy(ptr, &a, 8);
						i += 3;
						ptr += 8;
					}
				}
			}
			MemPage::CurrentPage->Close();
		}
		catch (...) {}
		MemPage::CurrentPage->Unlock();
	}
public:
	void operator=(const Delegate&) = delete;
	void operator=(Delegate&&) = delete;
	Delegate(const Delegate&) = delete;

	Delegate(Delegate&& ref) noexcept : invoker(ref.invoker), func(ref.func)
	{
		ref.invoker = nullptr;
		ref.func = nullptr;
		Init();
	}

	__declspec(noinline) explicit Delegate(Ret(*func)(Args...)) : invoker(new Function(func))
	{
		Init();
	}

	__declspec(noinline) explicit Delegate(const std::function<Ret(Args...)>& func) : invoker(new Function(func))
	{
		Init();
	}

	template<typename Cls>
	__declspec(noinline) Delegate(Cls& obj, Ret(Cls::* func)(Args...)) : invoker(new Method(obj, func))
	{
		Init();
	}

	template<typename Cls>
	__declspec(noinline) Delegate(Cls& obj, void* func) : invoker(new Method(obj, func))
	{
		Init();
	}

	Ret operator()(Args... args)
	{
		if (invoker == nullptr) throw std::exception("this object already be moved");
		return invoker->Invoke(std::forward<Args>(args)...);
	}

	//If want use this, please disable support just my code debugging (/JMC)
	Func GetFuncPtr()
	{
		return (Func)func->GetPtr();
	}

	~Delegate()
	{
		delete func;
		delete invoker;
	}
};
