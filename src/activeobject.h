#pragma once

// activeobject.h
//
// Create by Zhang Baoyan
//
#include <functional>
#include <memory>
#include <cassert>
#include "boost/thread/thread.hpp" 
#include "boost/noncopyable.hpp"
#include "concurrent_queue.h"

// use stand alone thread
class ActiveObjectThreaded : boost::noncopyable
{
public:

	typedef std::function<void()> Task;

	ActiveObjectThreaded() :m_done(false)
	{
		m_thd = std::unique_ptr<boost::thread>(
			new boost::thread(&ActiveObjectThreaded::ThreadLoop,this));
	}

	~ActiveObjectThreaded()
	{
		m_done = true;
		Task emptyTask;
		this->Submit(emptyTask);
		m_thd->join();
	}

	void Submit(Task& task)
	{
		m_taskQueue.push(task);
	}

private:
	ActiveObjectThreaded(const ActiveObjectThreaded&);// = default;
	ActiveObjectThreaded& operator=(const ActiveObjectThreaded&);// = default;

	bool m_done;
	concurrent_queue<Task> m_taskQueue;
	std::unique_ptr<boost::thread> m_thd;

	void ThreadLoop()
	{
		while (!m_done)
		{
			Task task;
			if (m_taskQueue.wait_and_pop(task) && task)
				task();
		}
	}
};

// do nothing
static void v_v_fn() {}
template<typename TContext>
static TContext* ptrT_v_fn() { return (TContext*)(0); }
// ʹ�ö����߳�
// ĳЩʱ��Ӹ����̴���Context�����̰߳�ȫ�ģ����̴߳��ݻ������
// �����ڴ��߳��ڴ���Context����ʱ����ʹ�ô��ֶ���
// fnContextCreator����, ���뷵�طǿյ�Context���󣬷����assert
template<typename TContext=void>
class ActiveObjectThreadedWithContext : boost::noncopyable
{
public:

	typedef std::function<void(TContext*)> Task;

	ActiveObjectThreadedWithContext(bool autoRun = true, std::function<TContext*()> funcContextCreator = ptrT_v_fn)
		:m_done(false), m_autoRun(autoRun), m_funcContextCreator(funcContextCreator)
	{
		if (mt_autoRun)
			m_thd = std::unique_ptr<boost::thread>(
				new boost::thread(&ActiveObjectThreaded::ThreadLoop, this));
	}

	~ActiveObjectThreadedWithContext()
	{
		m_done = true;
		Task emptyTask;
		this->Submit(emptyTask);
		m_thd->join();
	}

	void Run()
	{
		if (!m_autoRun && !m_thd)
			m_thd = std::unique_ptr<boost::thread>(
				new boost::thread(&ActiveObjectThreadedWithContext::ThreadLoop, this));
	}

	void Submit(Task& task)
	{
		m_taskQueue.push(task);
	}

private:
	
	ActiveObjectThreadedWithContext(const ActiveObjectThreadedWithContext&);
	ActiveObjectThreadedWithContext& operator=(const ActiveObjectThreadedWithContext&);

	bool m_done;
	bool mt_autoRun;
	concurrent_queue<Task> m_taskQueue;
	std::unique_ptr<boost::thread> m_thd;
	std::function<TContext*()> m_funcContextCreator;
	TContext* m_pContext;

	void ThreadLoop()
	{
		m_pContext = m_funcContextCreator();
		assert(m_pContext);
		while (!m_done)
		{
			Task task;
			if (m_taskQueue.wait_and_pop(task) && task)
				task(m_pContext);
		}
	}
};

// ʹ���̳߳�,����Ӧ���Դ����̳߳�
class ActiveObjectPooled : boost::noncopyable
{
public:
	
	typedef std::function<void()> Task;

	ActiveObjectPooled(){}

	void Submit(Task& task)
	{
		//s_threadpool_worker.submit(task);
	}

private:
	
	ActiveObjectPooled(const ActiveObjectPooled&);
	ActiveObjectPooled& operator=(const ActiveObjectPooled&);

};