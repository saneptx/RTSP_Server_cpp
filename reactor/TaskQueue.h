#ifndef __TASKQUEUE_H__
#define __TASKQUEUE_H__


#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
using std::queue;
using std::mutex;
using std::condition_variable;
using std::function;
using std::unique_lock;


using ElemType = function<void()>;
class TaskQueue{
public:
    TaskQueue(size_t queueSize);
    ~TaskQueue();
    void push(ElemType &&task);//添加任务
    ElemType pop();//获取任务

    bool isFull() const;//判满
    bool isEmpty() const;//判空
    void wakeUp();//唤醒所有等待在notEmpty的线程
private:
    size_t _queueSize;//任务队列大小
    queue<ElemType> _queue;//任务队列
    mutex _mutex;//互斥锁
    condition_variable _notFull;//非满条件变量
    condition_variable _notEmpty;//非空条件变量
    bool _notExit;
};

#endif