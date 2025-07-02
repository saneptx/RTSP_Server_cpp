#include "ThreadPool.h"
#include "Logger.h"

ThreadPool::ThreadPool(size_t threadNum,size_t queueSize)
:_threadNum(threadNum)
,_queueSize(queueSize)
,_taskQueue(_queueSize)
,_isExit(false)
{
    _threads.reserve(_threadNum);
    LOG_INFO("ThreadPool created - threads: %zu, queue size: %zu", threadNum, queueSize);
}

ThreadPool::~ThreadPool(){
    LOG_DEBUG("ThreadPool destructor called");
}

void ThreadPool::start(){
    LOG_INFO("Starting ThreadPool with %zu threads", _threadNum);
    //创建工作线程
    for(size_t idx = 0;idx != _threadNum;++idx){
        _threads.push_back(thread(&ThreadPool::doTask,this));
        LOG_DEBUG("Created worker thread %zu", idx);
    }
    LOG_INFO("ThreadPool started successfully");
}

void ThreadPool::stop(){
    LOG_INFO("Stopping ThreadPool...");
    while(!_taskQueue.isEmpty()){//如果任务队列不为空则需要等待任务执行完
        LOG_DEBUG("Waiting remaining tasks to complete");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    _isExit = true;
    _taskQueue.wakeUp();
    LOG_DEBUG("Joining %zu worker threads", _threads.size());
    for(auto &th :_threads){
        th.join();
    }
    LOG_INFO("ThreadPool stopped successfully");
}

void ThreadPool::addTask(Task &&task){
    if(task){
        LOG_DEBUG("Adding task to thread pool");
        _taskQueue.push(std::move(task));
    } else {
        LOG_WARN("Attempted to add null task to thread pool");
    }
}

Task ThreadPool::getTask(){
    Task task = _taskQueue.pop();
    if (task) {
        LOG_DEBUG("Retrieved task from queue");
    } else {
        LOG_DEBUG("No task available in queue");
    }
    return task;
}

void ThreadPool::doTask(){
    LOG_DEBUG("Worker thread %zu started", std::hash<std::thread::id>{}(std::this_thread::get_id()));
    
    while(!_isExit){
        
        Task task = getTask();

        if(task){
            LOG_DEBUG("Worker thread executing task");
            task();
        }else{
            LOG_DEBUG("Worker thread exiting");
        }
    }
    
    LOG_DEBUG("Worker thread %zu stopped", std::hash<std::thread::id>{}(std::this_thread::get_id()));
}