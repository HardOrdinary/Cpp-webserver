#include "threadpool.h"

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests),                                                                m_stop(false), m_threads(NULL)
{

    if ((thread_number <= 0 || (max_requests <= 0)))
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];

    if (!m_threads)
    {
        throw std::exception();
    }

    // 创建thread_number个线程，并将他们设置成线程脱离
    for (int i = 0; i < thread_number; i++)
    {
        printf("create the %dth thread\n", i);
        // worker是静态函数
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        // 成功返回0
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}
template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
// 静态变量类内声明，类外初始化，且多个对象共享静态函数和静态变量
template <typename T>
void *threadpool<T>::worker(void *arg)
{

    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_queuestat.wait(); // 信号量有值不阻塞(线程)
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request)
        {
            continue;
        }
        request->process();
    }
}