#ifndef __EVENTFD_H__
#define __EVENTFD_H__
#include <functional>
#include <mutex>
#include <sys/eventfd.h>
#include <unistd.h>
#include <vector>


using Functor = std::function<void()>;
class Eventor{
public:
    Eventor();
    ~Eventor();
    int getEvtfd();
    void handleRead();
    void addEventcb(Functor &&cb);
private:
    void doPenddingFunctors();
    int createEventFd();
    void wakeUp();
    int _evtfd;//用于通信的文件描述符
    std::vector<Functor> _pendings;//待执行的任务
    std::mutex _mutex;//互斥锁访问_pendings队列
};

#endif