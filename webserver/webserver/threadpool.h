#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <exception>
#include <cstdio>
#include "locker.h"

// 线程池类，定义成模板类是为了代码的复用, 模板参数T是任务类
template <typename T>
class threadpool
{
public:
    threadpool(int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request); // 添加任务

private:
    static void *worker(void *arg);
    void run();

private:
    // 线程的数量
    int m_thread_number;

    // 线程池数组，大小为m_thread_number
    pthread_t *m_threads;

    // 请求队列中最多允许的，等待处理的请求数量
    int m_max_requests;

    // 请求队列
    std::list<T *> m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    // 信号量用来判断是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程

    bool m_stop;
};




#endif