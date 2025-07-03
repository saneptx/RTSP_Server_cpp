#include "EventLoop.h"
#include <sys/epoll.h>
#include "Acceptor.h"
#include <unistd.h>
#include <iostream>
#include <sys/eventfd.h>
#include "TcpConnection.h"
#include "UdpConnection.h"
#include <memory>
#include <cassert>
#include <string.h>
#include "Logger.h"

using std::cout;
using std::endl;
using std::cerr;
using std::unique_lock;

EventLoop::EventLoop(Acceptor &acceptor, bool isMainLoop)
:_epfd(createEpollFd())
,_evtList(1024)
,_isLooping(false)
,_acceptor(acceptor)
,_conns()
,_eventor()//创建用于通信的文件描述符
,_threadId() // 初始化为空
{
    if (isMainLoop) {
        int listenfd = acceptor.fd();
        addEpollReadFd(listenfd);
        LOG_INFO("Main EventLoop created, listening on fd: %d", listenfd);
    }
    else{
        LOG_INFO("Sub EventLoop created");
        addEpollReadFd(_eventor.getEvtfd());
        addEpollReadFd(_timeMgr.getTimerFd());
    }
}
EventLoop::~EventLoop(){
    LOG_DEBUG("EventLoop destructor called, closing epoll fd: %d", _epfd);
    close(_epfd);
}

void EventLoop::loop(){
    _threadId = std::this_thread::get_id();  // 记录当前线程ID
    _isLooping = true;
    LOG_INFO("EventLoop started in thread: %zu", std::hash<std::thread::id>{}(_threadId));
    while(_isLooping){
        waitEpollFd();
    }
    LOG_INFO("EventLoop stopped in thread: %zu", std::hash<std::thread::id>{}(_threadId));
}
void EventLoop::unloop(){
    LOG_INFO("EventLoop unloop called");
    _isLooping = false;
}

void EventLoop::assertInLoopThread() {
    if (!isInLoopThread()) {
        LOG_ERROR("EventLoop::assertInLoopThread - EventLoop was created in threadId = %zu, current thread id = %zu", 
                 std::hash<std::thread::id>{}(_threadId), std::hash<std::thread::id>{}(std::this_thread::get_id()));
        abort();
    }
}

void EventLoop::addConnection(const TcpConnectionPtr& conn) {
    assertInLoopThread();
    int fd = conn->getFd();

    _conns[fd] = conn;
    addEpollReadFd(fd);
    LOG_DEBUG("Added connection fd: %d, total connections: %zu", fd, _conns.size());
}

void EventLoop::removeConnection(const TcpConnectionPtr& conn) {
    assertInLoopThread();
    int fd = conn->getFd();
    _conns.erase(fd);
    delEpollReadFd(fd);
    LOG_DEBUG("Removed connection fd: %d, remaining connections: %zu", fd, _conns.size());
}

void EventLoop::waitEpollFd(){

    int nready = 0;
    do{
        nready = epoll_wait(_epfd,&*_evtList.begin(),_evtList.size(),3000);
    }while(-1 == nready && errno == EINTR);
    if(-1 == nready){
        LOG_ERROR("epoll_wait failed: %s", strerror(errno));
        return; 
    }else if(0 == nready){
        LOG_DEBUG("epoll_wait timeout, thread id: %zu", std::hash<std::thread::id>{}(std::this_thread::get_id()));
    }else{
        // LOG_DEBUG("epoll_wait returned %d events", nready);
        //判断一下文件描述符是不是到1024了
        //如果达到1024就需要扩容
        if(nready == (int)_evtList.size()){
            _evtList.reserve(2 * nready);
            LOG_DEBUG("Expanded event list to %zu", _evtList.capacity());
        }
        for(int idx = 0;idx < nready; ++idx){
            int fd = _evtList[idx].data.fd;
            uint32_t events = _evtList[idx].events;
            if(fd == _acceptor.fd()){//处理当有客户端连接时
                if(events & EPOLLIN){
                    LOG_DEBUG("New connection event on acceptor fd: %d", fd);
                    handleNewConnection();
                }
            }else if(fd == _eventor.getEvtfd()){//处理触发事件响应
                if(events & EPOLLIN){
                    LOG_DEBUG("Eventor event on fd: %d", fd);
                    _eventor.handleRead();
                }
            }else if(fd == _timeMgr.getTimerFd()){//处理时间响应任务
                if(events & EPOLLIN){
                    // LOG_DEBUG("Timer event on fd: %d", fd);
                    _timeMgr.handleRead();
                }
            }else{
                if(events & EPOLLIN){
                    // LOG_DEBUG("Read event on fd: %d", fd);
                    handleMessage(fd);
                }
                if(events & EPOLLOUT){
                    // LOG_DEBUG("Write event on fd: %d", fd);
                    auto itTcp = _conns.find(fd);
                    if(itTcp != _conns.end()){
                        itTcp->second->handleWriteCallback();
                    }
                }
            }
        }
    }
}
void EventLoop::handleNewConnection(){
    int connfd = _acceptor.accept();
    if(connfd < 0){
        LOG_ERROR("handleNewConnection accept failed: %s", strerror(errno));
        return;
    }
    LOG_INFO("New connection accepted, fd: %d", connfd);
    _onNewConnectionCb(connfd);
}
void EventLoop::handleMessage(int fd){
    auto itTcp = _conns.find(fd);
    auto itUdp = udpConns.find(fd);
    if(itTcp != _conns.end()){
        if(itTcp->second->isClosed()){
            LOG_DEBUG("Connection fd: %d is closed, handling close callback", fd);
            itTcp->second->handleCloseCallback();
            removeConnection(itTcp->second);
        }else{
            itTcp->second->handleMessageCallback();
        }
    }else if(itUdp != udpConns.end()){
        LOG_DEBUG("Handling UDP message for fd: %d", fd);
        itUdp->second->handleMessageCallback();//处理udp消息
    }else{
        LOG_ERROR("Connection not found for fd: %d", fd);
        return;
    }
}
int EventLoop::createEpollFd(){
    int fd = ::epoll_create1(0);
    if(fd < 0){
        LOG_ERROR("epoll_create failed: %s", strerror(errno));
        return fd;
    }
    LOG_DEBUG("Created epoll fd: %d", fd);
    return fd;
}
void EventLoop::addEpollReadFd(int fd){
    struct  epoll_event evt;
    evt.events = EPOLLIN;
    evt.data.fd = fd;
    
    int ret = ::epoll_ctl(_epfd,EPOLL_CTL_ADD,fd,&evt);
    if(ret < 0){
        LOG_ERROR("epoll_ctl_add failed for fd %d: %s", fd, strerror(errno));
        return;
    }
    LOG_DEBUG("Added fd %d to epoll read events", fd);
    
}
void EventLoop::delEpollReadFd(int fd){
    struct  epoll_event evt;
    evt.data.fd = fd;
    
    int ret = ::epoll_ctl(_epfd,EPOLL_CTL_DEL,fd,&evt);
    if(ret < 0){
        LOG_ERROR("epoll_ctl_del failed for fd %d: %s", fd, strerror(errno));
        return;
    }
    LOG_DEBUG("Removed fd %d from epoll read events", fd);
}

void EventLoop::addEpollWriteFd(int fd){
    struct epoll_event evt;
    evt.events = EPOLLOUT | EPOLLIN;
    evt.data.fd = fd;
    int ret = ::epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &evt);
    if(ret < 0){
        LOG_ERROR("epoll_ctl_mod (add write) failed for fd %d: %s", fd, strerror(errno));
        return;
    }
    LOG_DEBUG("Added write event for fd %d", fd);
}
void EventLoop::delEpollWriteFd(int fd){
    struct epoll_event evt;
    evt.events = EPOLLIN;
    evt.data.fd = fd;
    int ret = ::epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &evt);
    if(ret < 0){
        LOG_ERROR("epoll_ctl_mod (del write) failed for fd %d: %s", fd, strerror(errno));
        return;
    }
    LOG_DEBUG("Removed write event for fd %d", fd);
}

void EventLoop::setNewConnectionCallback(std::function<void(int)> &&cb){
    _onNewConnectionCb = std::move(cb);
    LOG_DEBUG("New connection callback set");
}
void EventLoop::setMessageCallback(TcpConnectionCallback &&cb){
    _onMessageCb = std::move(cb);
    LOG_DEBUG("Message callback set");
}
void EventLoop::setCloseCallback(TcpConnectionCallback &&cb){
    _onCloseCb = std::move(cb);
    LOG_DEBUG("Close callback set");
}

void EventLoop::runInLoop(Functor &&cb){
    if (isInLoopThread()) {
        // LOG_DEBUG("Running function in current loop thread");
        cb();
    } else {
        LOG_DEBUG("Queuing function to run in loop thread");
        _eventor.addEventcb(std::move(cb));
    }
}

TimerId EventLoop::addOneTimer(int delaySec, TimerCallback &&cb){
    LOG_DEBUG("Adding one-shot timer with delay: %d seconds", delaySec);
    return _timeMgr.addTimer(delaySec,std::move(cb));
}

TimerId EventLoop::addPeriodicTimer(int delaySec, int intervalSec, TimerCallback &&cb){
    LOG_DEBUG("Adding periodic timer with delay: %d seconds, interval: %d seconds", delaySec, intervalSec);
    return _timeMgr.addPeriodicTimer(delaySec, intervalSec, std::move(cb));
}

void EventLoop::removeTimer(TimerId timerId){
    LOG_DEBUG("Removing timer: %lu", timerId);
    _timeMgr.removeTimer(timerId);
}