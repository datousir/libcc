//
//  thread_joiner.h
//
//  Created by Zhang Baoyan on 14-04-21.
//
#ifndef _THREADS_JOINER_H_
#define _THREADS_JOINER_H_

#include <vector>
#include "boost/thread/thread.hpp"

class thread_joiner
{
    std::vector<boost::thread>& m_threads;
public:
    explicit thread_joiner(std::vector<boost::thread>& threads):
    m_threads(threads)
    {}
    ~thread_joiner()
    {
        for ( size_t i=0;i<m_threads.size();++i)
        {
            if(m_threads[i].joinable())
                m_threads[i].join();
        }
    }
};

#endif
