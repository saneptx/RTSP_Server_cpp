#ifndef __EVENTLOOP_H__
#define __EVENTLOOP_H__

#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>

using std::vector;
using std::map;
using std::shared_ptr;
using std::function;
using std::mutex;

class Acceptor;
class TcpConnection;
using TcpConnectionPtr = shared_ptr<TcpConnection>;
using TcpConnectionCallback = function<void(const TcpConnectionPtr &)>;
using Functor = function<void()>;

class EventLoop{
public:
    EventLoop(Acceptor &acceptor);
    ~EventLoop();

    void loop();
    void unloop();

    void setNewConnectionCallback(TcpConnectionCallback &&cb);
    void setMessageCallback(TcpConnectionCallback &&cb);
    void setCloseCallback(TcpConnectionCallback &&cb);

    void wakeup();
    void runInLoop(Functor &&cb);

private:
    void waitEpollFd();
    void handleNewConnection();
    void handleMessage(int fd);
    void addEpollReadFd(int fd);
    void delEpollReadFd(int fd);
    int createEpollFd();

    void handleRead();
    void doPenddingFunctors();
    int createEventFd();

private:
    int _epfd;
    vector<struct epoll_event> _evtList;
    bool _isLooping;
    Acceptor &_acceptor;
    map<int,TcpConnectionPtr> _conns;

    TcpConnectionCallback _onNewConnectionCb;
    TcpConnectionCallback _onMessageCb;
    TcpConnectionCallback _onCloseCb;

    int _evtfd;//用于通信的文件描述符
    vector<Functor> _pendings;//待执行的任务
    mutex _mutex;//互斥锁访问_pendings队列
};

#endif