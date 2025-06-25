#include "TaskQueue.h"

TaskQueue::TaskQueue(size_t queueSize)
:_queueSize(queueSize)
,_queue()
,_mutex()
,_notFull()
,_notEmpty()
,_notExit(true){

}
TaskQueue::~TaskQueue(){

}
void TaskQueue::push(ElemType &&task){

    unique_lock<mutex> autoLock(_mutex);

    while(isFull()){//使用while循环避免虚假唤醒
        _notFull.wait(autoLock);
    }

    _queue.push(std::move(task));
    _notEmpty.notify_one();

}
ElemType TaskQueue::pop(){

    unique_lock<mutex> autoLock(_mutex);
    while(isEmpty() & _notExit){//没有任务且不退出则等待新的任务
        _notEmpty.wait(autoLock);
    }
    if(_notExit){
        ElemType tmp = _queue.front();
        _queue.pop();
        _notFull.notify_one();
        return tmp;
    }else{
        return nullptr;
    }
}

bool TaskQueue::isFull() const{
    return _queue.size() == _queueSize;
}
bool TaskQueue::isEmpty() const{
    return _queue.size() == 0;
}
void TaskQueue::wakeUp(){
    _notExit = false; //决定退出，唤醒所有等待新任务的线程进入退出流程
    _notEmpty.notify_all();
}