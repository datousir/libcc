#pragma once
#include <memory>
#include <utility>

// 只满足移动语义，不满足拷贝语义
class Callable
{
	struct CallableBase
	{
		virtual void call() = 0;
		virtual ~CallableBase() {}
	};

	template<typename Func>
	struct CallableTyped:CallableBase
	{
		Func m_callable;
		CallableTyped(Func&& f):m_callable(std::move(f)){}
		void call() { m_callable(); }
	};

public:
	template<typename Func>
	explicit Callable(Func&& f)
		:m_callable(new CallableTyped<Func>(std::move(f)))
	{
	}

	Callable() {}

	Callable(Callable&& callable)
		:m_callable(std::move(callable.m_callable))
	{
		callable.m_callable = 0;
	}

	Callable& operator=(Callable&& callable)
	{
		m_callable = std::move(callable.m_callable);
		callable.m_callable = 0;
		return *this;
	}

	void operator()() { m_callable->call(); }

private:

	Callable(const Callable&);
	Callable& operator=(const Callable&);

	std::unique_ptr<CallableBase> m_callable;
};