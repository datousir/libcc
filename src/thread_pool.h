//
//  thread_pool.h
//
//  Created by Zhang Baoyan on 14-04-21.
//



#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <vector>
#include <deque>
#include <functional>
#include <type_traits>
#include "boost/atomic/atomic.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"
#include "boost/thread/tss.hpp"
#include "boost/noncopyable.hpp"

#include "concurrent_queue.h"
#include "thread_joiner.h"
#include "callable.h"

template<typename T>
class work_stealing_queue
{
private:
    std::deque<T> the_queue;
    mutable boost::mutex the_mutex;

    work_stealing_queue(const work_stealing_queue& other);
    work_stealing_queue& operator=(const work_stealing_queue& other);
public:
    work_stealing_queue(){}

	// copy
    void push(T data)
    {
        boost::lock_guard<boost::mutex> lock(the_mutex);
        the_queue.push_front(boost::move(data));
    }

    bool empty() const
    {
        boost::lock_guard<boost::mutex> lock(the_mutex);
        return the_queue.empty();
    }

    bool try_pop(T& res)
    {
        boost::lock_guard<boost::mutex> lock(the_mutex);

        if(the_queue.empty())
            return false;

        res = boost::move(the_queue.front());
        the_queue.pop_front();
        return true;
    }

    bool try_steal(T& res)
    {
        boost::lock_guard<boost::mutex> lock(the_mutex);

        if (the_queue.empty())
            return false;

        res = boost::move(the_queue.back());
        the_queue.pop_back();
        return true;
    }
};


// do nothing
static void void_void_fn(){}

template<typename T>
static T* pret_void_fn(){}

// *ONLY* accept std::function<void()> function
// You can use std::bind to adapt other functions
class thread_pool
{

public:

    typedef std::function<void()> task_type;

private:
    boost::atomic_bool done;
    std::vector<boost::thread> threads;
    thread_joiner joiner;
    task_type fn_init_pre_context;
    int pool_size;
    bool use_threadlocal_queue;

    concurrent_queue<task_type> pool_work_queue;
    std::vector<std::unique_ptr<work_stealing_queue<task_type>>> queues;
    boost::thread_specific_ptr<work_stealing_queue<task_type>> local_work_queue;
    boost::thread_specific_ptr<int> my_index;

    void thread_loop(int myIndex)
    {
        my_index.reset(new int(myIndex));
        local_work_queue.reset(queues[*my_index].get());
        fn_init_pre_context();

        while(!done)
        {
            run_pending_task();
        }
    }

    bool pop_task_from_local_queue(task_type& task)
    {
        return local_work_queue.get() && local_work_queue->try_pop(task);
    }

    bool pop_task_from_pool_queue(task_type& task)
    {
        //return pool_work_queue.try_pop(task);
        return use_threadlocal_queue
            ?pool_work_queue.try_pop(task)
            :pool_work_queue.wait_and_pop(task);
    }

    bool pop_task_from_other_thread_queue(task_type& task)
    {
        // There's only one thread.
        if(pool_size == 1)
            return false;

        for (size_t i=0; i<queues.size()-1; ++i)
        {
            const unsigned int index = ((*my_index)+i+1)%queues.size();
            if (queues[index]->try_steal(task))
                return true;
        }
        return false;
    }

public:
    thread_pool(int poolSize = 0,
        std::function<void()>fnInitPreCtx = void_void_fn,
        bool useThreadlocalQueue=false)
        :done(false),joiner(threads),fn_init_pre_context(fnInitPreCtx),
        pool_size(poolSize),use_threadlocal_queue(useThreadlocalQueue)
    {
    }

    void run(int poolSize = 0)
    {
        pool_size = poolSize;
        if(pool_size <= 0)
            pool_size = boost::thread::hardware_concurrency();

        try
        {
            // 必须先创建队列,全部创建完毕后再启动线程,因为queue.size()不是线程安全的
            for (int i = 0; i < pool_size; ++i)
            {
                queues.push_back(std::unique_ptr<work_stealing_queue<task_type>>(
                    new work_stealing_queue<task_type>));
            }
            for (int i = 0; i < pool_size; ++i)
            {
                threads.push_back(
                    boost::thread(&thread_pool::thread_loop,this,i));
            }
        }
        catch (...)
        {
            done = true;
            throw;

        }
    }

    ~thread_pool()
    {
        done = true;
    }

    void submit(task_type& fn_task)
    {
		// 只有worker线程的local_work_queue是有效的
        if (use_threadlocal_queue && local_work_queue.get())
        {
            local_work_queue->push(fn_task);
        }
        else
        {
            pool_work_queue.push(fn_task);
        }
    }

    void run_pending_task()
    {
        task_type task;

        if((use_threadlocal_queue && pop_task_from_local_queue(task))
            || pop_task_from_pool_queue(task)
            ||(use_threadlocal_queue && pop_task_from_other_thread_queue(task)))
        {
            task();
        }
        else
        {
            boost::this_thread::yield();
        }
    }
};


// *ONLY* accept std::function<void(Context*)> function
// You can use std::bind to adapt other functions
template<typename Context>
class thread_pool_with_context
{
public:
    typedef std::function<void(Context*)> task_type;
private:
    boost::atomic_bool done;
    std::vector<boost::thread> threads;
    thread_joiner joiner;
    std::function<Context*()> fn_init_pre_context;
    int pool_size;
    bool use_threadlocal_queue;

    concurrent_queue<task_type> pool_work_queue;
    std::vector<std::unique_ptr<work_stealing_queue<task_type>>> queues;
    static boost::thread_specific_ptr<work_stealing_queue<task_type>> local_work_queue;
    static boost::thread_specific_ptr<int> my_index;
    static boost::thread_specific_ptr<Context> pre_context;


    void thread_loop(int myIndex)
    {
        my_index.reset(new int(myIndex));
        local_work_queue.reset(queues[*my_index].get());
        pre_context.reset(fn_init_pre_context());

        while(!done)
        {
            run_pending_task();
        }
    }

    bool pop_task_from_local_queue(task_type& task)
    {
        return local_work_queue.get() && local_work_queue->try_pop(task);
    }

    bool pop_task_from_pool_queue(task_type& task)
    {
        //当使用县城局部队列时,线程池队列task使用非阻塞形式,否则使用阻塞形式
        return use_threadlocal_queue
            ?pool_work_queue.try_pop(task)
            :pool_work_queue.wait_and_pop(task);
    }

    bool pop_task_from_other_thread_queue(task_type& task)
    {
        // There's only one thread.
        if(pool_size == 1)
            return false;

        for (size_t i=0;i<queues.size()-1;++i)
        {
            const int index = ((*my_index)+i+1)%queues.size();
            if (queues[index]->try_steal(task))
                return true;
        }
        return false;
    }

    void run_pending_task()
    {
        task_type task;

        if((use_threadlocal_queue && pop_task_from_local_queue(task))
            || pop_task_from_pool_queue(task)
            ||(use_threadlocal_queue && pop_task_from_other_thread_queue(task)))
        {
            task(pre_context.get());
        }
        else
        {
            boost::this_thread::yield();
        }
    }
    
public:
    thread_pool_with_context(int poolSize = 0,
        std::function<Context*()> fnInitPreCtx = pret_void_fn<Context>,
        bool useThreadlocalQueue=false)
        :done(false),joiner(threads),fn_init_pre_context(fnInitPreCtx),
        pool_size(poolSize),use_threadlocal_queue(useThreadlocalQueue)
    {
    }

    ~thread_pool_with_context()
    {
        done = true;
    }

    void run(int poolSize = 0)
    {
        pool_size = poolSize;
        if(pool_size <= 0)
            pool_size = boost::thread::hardware_concurrency();

        try
        {
            // 必须先创建队列,全部创建完毕后再启动线程,因为queue.size()不是线程安全的
            for (int i = 0; i < pool_size; ++i)
            {
                queues.push_back(std::unique_ptr<work_stealing_queue<task_type>>(
                    new work_stealing_queue<task_type>));
            }

            for (int i = 0; i < pool_size; ++i)
            {
                threads.push_back(
                    boost::thread(&thread_pool_with_context::thread_loop,this,i));
            }
        }
        catch (...)
        {
            done = true;
            throw;

        }
    }

    void submit(task_type& fn_task)
    {
        if (use_threadlocal_queue && local_work_queue.get())
        {
            local_work_queue->push(fn_task);
        }
        else
        {
            pool_work_queue.push(fn_task);
        }
    }

    Context* getCurrentContext() const
    {
        return pre_context.get();
    }
};

template<typename Context>
boost::thread_specific_ptr<int> thread_pool_with_context<Context>::my_index;

template<typename Context>
boost::thread_specific_ptr<Context> thread_pool_with_context<Context>::pre_context;

template<typename Context>
boost::thread_specific_ptr<work_stealing_queue<typename thread_pool_with_context<Context>::type_type>>
thread_pool_with_context<Context>::local_work_queue;


// 可以等待返回值的线程池
class thread_pool_retval : boost::noncopyable
{
public:

	typedef Callable task_type;

private:
	boost::atomic_bool done;
	std::vector<boost::thread> threads;
	thread_joiner joiner;
	std::function<void()> fn_init_pre_context;
	int pool_size;

	concurrent_queue<task_type> pool_work_queue;

	void run_pending_task()
	{
		task_type task;

		if (pool_work_queue.wait_and_pop(task))
		{
			task();
		}
		else
		{
			boost::this_thread::yield();
		}
	}

	void thread_loop(int myIndex)
	{
		fn_init_pre_context();

		while (!done)
		{
			run_pending_task();
		}
	}

public:
	thread_pool_retval(int poolSize = 0,
		std::function<void()>fnInitPreCtx = void_void_fn)
		:done(false), joiner(threads), fn_init_pre_context(fnInitPreCtx),
		pool_size(poolSize)
	{
	}

	void run(int poolSize = 0)
	{
		pool_size = poolSize;
		if (pool_size <= 0)
			pool_size = boost::thread::hardware_concurrency();
		
		try
		{
			for (int i = 0; i < pool_size; ++i)
			{
				threads.push_back(
					boost::thread(&thread_pool_retval::thread_loop, this, i));
			}
		}
		catch (...)
		{
			done = true;
			throw;

		}
	}

	~thread_pool_retval()
	{
		done = true;
	}

	template<typename Func>
	boost::future<typename std::result_of<Func()>::type>
		submit(Func f)
	{
		typedef typename std::result_of<Func()>::type
			result_type_t;

		boost::packaged_task<result_type_t()> task(boost::move(f));
		boost::future<result_type_t> ret(task.get_future());
		
		pool_work_queue.push(boost::move(Callable(boost::move(task))));

		return ret;
	}

};
///////////////////////////////////////////////////////////////////////
// Testing Code
///////////////////////////////////////////////////////////////////////


//void printHello()
//{
//    printf("Hello,world!\n");
//}
//void async_add(int a,int b,boost::promise<int>* result)
//{
//    result->set_value(a+b);
//}

//thread_pool thread_pools;
// thread_pools.run();
//thread_pools.submit(printHello);

//boost::promise<int> presult;
//boost::future<int> fresult;
//int a = 11212121,b = 32334234;

//thread_pools.submit(std::bind(async_add,a,b,&presult));
//fresult = presult.get_future();
//fresult.wait();
//if(fresult.has_value())
//    printf("%d + %d value is %d\n",a,b,fresult.get());
//sleep(10);


//int add(int a, int b)
//{
//	return a + b;
//}

//thread_pool_retval tpr(4);
//tpr.run(2);

//auto fu_result = tpr.submit(std::bind(add, 3, 4));

//fprintf(stdout, " 3 + 4 = %d", fu_result.get());


//////////////////////////////////////////////////////////////////////////
#endif
