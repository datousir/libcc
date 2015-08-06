//
//  concurrent_queue.h
//
//  Created by Zhang Baoyan on 14-04-21.
//

#ifndef _CONCURRENT_QUEUE_H_
#define _CONCURRENT_QUEUE_H_

#include "boost/thread/mutex.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/move/move.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/lock_guard.hpp"
#include <functional>
#include <memory>

template<typename T>
class concurrent_queue
{
private:

    struct node;
    struct node_deleter // workaround for vs2010
    {
        void operator()(node* ptr) const
        {
            delete ptr;
        }
    };
    struct node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<node,node_deleter> next;
    };

    std::unique_ptr<node,node_deleter> head;

    boost::mutex head_mutex;

    node* tail;
    boost::mutex tail_mutex;
    boost::condition_variable data_cond;

    concurrent_queue(const concurrent_queue& other);
    concurrent_queue& operator=(const concurrent_queue& other);


    node* get_tail()
    {
        boost::lock_guard<boost::mutex> tail_lock(tail_mutex);
        return tail;
    }
    bool has_node()
    {
        return head.get() != get_tail();
    }
    std::unique_ptr<node,node_deleter> pop_head()
    {
        std::unique_ptr<node,node_deleter> old_head = boost::move(head);
        head = boost::move(old_head->next);
        return old_head;
    }
    boost::unique_lock<boost::mutex> wait_for_data()
    {
        boost::unique_lock<boost::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock,std::bind(&concurrent_queue<T>::has_node,this));
        return boost::move(head_lock);
    }
    std::unique_ptr<node,node_deleter> wait_pop_head()
    {
        boost::unique_lock<boost::mutex> head_lock(wait_for_data());
        return pop_head();
    }
    std::unique_ptr<node,node_deleter> wait_pop_head(T& value)
    {
        boost::unique_lock<boost::mutex> head_lock(wait_for_data());
        value = boost::move(*head->data);
        return pop_head();
    }
    std::unique_ptr<node,node_deleter> try_pop_head()
    {
        boost::lock_guard<boost::mutex> head_lock(head_mutex);
        if(!has_node())
            return std::unique_ptr<node>();
        return pop_head();
    }
    std::unique_ptr<node,node_deleter> try_pop_head(T& value)
    {
        boost::lock_guard<boost::mutex> head_lock(head_mutex);
        if(!has_node())
            return std::unique_ptr<node,node_deleter>();
        value = boost::move(*head->data);
        return pop_head();
    }

public:
	// insert a empty node when construct the queue
    concurrent_queue():
        head(new node),tail(head.get()){}

    std::shared_ptr<T> try_pop()
    {
        std::unique_ptr<node,node_deleter> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }

    bool try_pop(T& value)
    {
        std::unique_ptr<node,node_deleter> const old_head
            = try_pop_head(value);
        return old_head;
    }

    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_ptr<node,node_deleter> const old_head
            = wait_pop_head();
        return old_head->data;
    }

    bool wait_and_pop(T& value)
    {
        std::unique_ptr<node,node_deleter> const old_head
            = wait_pop_head(value);
        return old_head;
    }

    void push (T new_value)
    {
        std::shared_ptr<T> new_data(
            std::make_shared<T>(boost::move(new_value)));

        std::unique_ptr<node,node_deleter> p(new node);
        {
            boost::lock_guard<boost::mutex> tail_lock(tail_mutex);
            tail->data = new_data;
            node* const new_tail = p.get();
            tail->next = boost::move(p);
            tail = new_tail;
        }
        data_cond.notify_one();
    }

    bool empty()
    {
        boost::lock_guard<boost::mutex> head_lock(head_mutex);
        return !has_node();
    }

};

#endif
