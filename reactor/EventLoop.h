#ifndef __EVENTLOOP_H__
#define __EVENTLOOP_H__

#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include "Eventor.h"
#include "TimerManager.h"

using std::vector;
using std::map;
using std::shared_ptr;
using std::function;
using std::mutex;

class Acceptor;
class TcpConnection;
class UdpConnection;
using UdpConnectionPtr = std::shared_ptr<UdpConnection>;
using TcpConnectionPtr = shared_ptr<TcpConnection>;
using TcpConnectionCallback = function<void(const TcpConnectionPtr &)>;
using Functor = function<void()>;
using TimerCallback = std::function<void()>;
using TimerId = uint64_t;

class EventLoop{
public:
    EventLoop(Acceptor &acceptor, bool isMainLoop = false);
    ~EventLoop();

    void loop();
    void unloop();

    void setNewConnectionCallback(std::function<void(int)> &&cb);
    void setMessageCallback(TcpConnectionCallback &&cb);
    void setCloseCallback(TcpConnectionCallback &&cb);
    TimerId addOneTimer(int delaySec, TimerCallback &&cb);
    TimerId addPeriodicTimer(int delaySec, int intervalSec, TimerCallback &&cb);
    void removeTimer(TimerId timerId);
    void runInLoop(Functor &&cb);
    void addEpollReadFd(int fd);
    void delEpollReadFd(int fd);
    void addEpollWriteFd(int fd);
    void delEpollWriteFd(int fd);
    
    
    bool isInLoopThread() const { return _threadId == std::this_thread::get_id(); }
    void assertInLoopThread();
    
    
    void addConnection(const TcpConnectionPtr& conn);
    void removeConnection(const TcpConnectionPtr& conn);
    
    map<int,UdpConnectionPtr> udpConns;

private:
    void waitEpollFd();
    void handleNewConnection();
    void handleMessage(int fd);
    int createEpollFd();

private:
    int _epfd;
    vector<struct epoll_event> _evtList;
    bool _isLooping;
    Acceptor &_acceptor;
    map<int,TcpConnectionPtr> _conns;

    std::function<void(int)> _onNewConnectionCb;
    TcpConnectionCallback _onMessageCb;
    TcpConnectionCallback _onCloseCb;

    Eventor _eventor;
    TimerManager _timeMgr;
    
    std::thread::id _threadId;  // 记录当前EventLoop运行的线程ID
};

#endif