#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include "TaskQueue.h"
#include <vector>
#include <thread>
#include <iostream>

using std::cout;
using std::endl;
using std::vector;
using std::thread;


class Thread;
using Task = function<void()>;
class ThreadPool{
public:
    ThreadPool(size_t threadNum,size_t queueSize);
    ~ThreadPool();
    void start();
    void stop();
    void addTask(Task &&task);
    void doTask();
private:
    Task getTask();
    size_t _threadNum;
    size_t _queueSize;
    TaskQueue _taskQueue;
    bool _isExit;
    vector<thread> _threads;
};


#endif