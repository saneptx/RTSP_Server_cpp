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
    }
    else{
        addEpollReadFd(_eventor.getEvtfd());
        addEpollReadFd(_timeMgr.getTimerFd());
    }
}
EventLoop::~EventLoop(){
    close(_epfd);
}

void EventLoop::loop(){
    _threadId = std::this_thread::get_id();  // 记录当前线程ID
    _isLooping = true;
    while(_isLooping){
        waitEpollFd();
    }
}
void EventLoop::unloop(){
    cout<<"unloop()"<<endl;
    _isLooping = false;
}

void EventLoop::assertInLoopThread() {
    if (!isInLoopThread()) {
        cerr << "EventLoop::assertInLoopThread - EventLoop was created in threadId = " 
             << _threadId << ", current thread id = " << std::this_thread::get_id() << endl;
        abort();
    }
}

void EventLoop::addConnection(const TcpConnectionPtr& conn) {
    assertInLoopThread();
    int fd = conn->getFd();
    _conns[fd] = conn;
    addEpollReadFd(fd);
}

void EventLoop::removeConnection(const TcpConnectionPtr& conn) {
    assertInLoopThread();
    int fd = conn->getFd();
    _conns.erase(fd);
    delEpollReadFd(fd);
}

void EventLoop::waitEpollFd(){

    int nready = 0;
    do{
        nready = epoll_wait(_epfd,&*_evtList.begin(),_evtList.size(),3000);
    }while(-1 == nready && errno == EINTR);
    if(-1 == nready){
        cerr<<"-1 == nready" <<endl;
        return; 
    }else if(0 == nready){
        cout<<">>epoll_wait timeout,thread id:"<<std::this_thread::get_id()<<endl;
    }else{
        //判断一下文件描述符是不是到1024了
        //如果达到1024就需要扩容
        if(nready == (int)_evtList.size()){
            _evtList.reserve(2 * nready);
        }
        for(int idx = 0;idx < nready; ++idx){
            int fd = _evtList[idx].data.fd;
            if(fd == _acceptor.fd()){//处理当有客户端连接时
                if(_evtList[idx].events & EPOLLIN){
                    handleNewConnection();
                }
            }else if(fd == _eventor.getEvtfd()){//处理事件响应
                if(_evtList[idx].events & EPOLLIN){
                    _eventor.handleRead();
                }
            }else if(fd == _timeMgr.getTimerFd()){//处理时间响应任务
                if(_evtList[idx].events & EPOLLIN){
                    _timeMgr.handleRead();
                }
            }else{//处理客户端有消息到来
                if(_evtList[idx].events & EPOLLIN){
                    handleMessage(fd);
                }
            }
        }
    }
}
void EventLoop::handleNewConnection(){
    cout<<"handleNewConnection"<<endl;
    int connfd = _acceptor.accept();
    if(connfd < 0){
        perror("handleNewConnection accpet");
        return;
    }
    TcpConnectionPtr connPtr(new TcpConnection(connfd,this));
    _onNewConnectionCb(connPtr); // 只分配，不监听
}
void EventLoop::handleMessage(int fd){
    cout<<"handleMessage"<<endl;
    auto itTcp = _conns.find(fd);
    auto itUdp = udpConns.find(fd);
    if(itTcp != _conns.end()){
        //连接存在可以进行收发消息
        bool flag = itTcp->second->isClosed();
        if(flag){
            itTcp->second->handleCloseCallback();
            removeConnection(itTcp->second);//断开连接后删除监听
        }else{
            cout<<"handleMessageCallback"<<endl;
            itTcp->second->handleMessageCallback();
        }
    }else if(itUdp != udpConns.end()){
        itUdp->second->handleMessageCallback();//处理udp消息
    }else{
        //连接不存在，直接让程序退出
        cout<<"连接不存在！"<<endl;
        return;
    }
}
int EventLoop::createEpollFd(){
    int fd = ::epoll_create1(0);
    if(fd < 0){
        perror("epoll_create");
        return fd;
    }
    return fd;
}
void EventLoop::addEpollReadFd(int fd){
    struct  epoll_event evt;
    evt.events = EPOLLIN;
    evt.data.fd = fd;
    
    int ret = ::epoll_ctl(_epfd,EPOLL_CTL_ADD,fd,&evt);
    if(ret < 0){
        perror("epoll_ctl_add");
        return;
    }
    
}
void EventLoop::delEpollReadFd(int fd){
    struct  epoll_event evt;
    evt.data.fd = fd;
    
    int ret = ::epoll_ctl(_epfd,EPOLL_CTL_DEL,fd,&evt);
    if(ret < 0){
        perror("epoll_ctl_del");
        return;
    }
}

void EventLoop::setNewConnectionCallback(TcpConnectionCallback &&cb){
    _onNewConnectionCb = std::move(cb);
}
void EventLoop::setMessageCallback(TcpConnectionCallback &&cb){
    _onMessageCb = std::move(cb);
}
void EventLoop::setCloseCallback(TcpConnectionCallback &&cb){
    _onCloseCb = std::move(cb);
}

void EventLoop::runInLoop(Functor &&cb){
    if (isInLoopThread()) {
        cb();
    } else {
        _eventor.addEventcb(std::move(cb));
    }
}

TimerId EventLoop::addOneTimer(int delaySec, TimerCallback &&cb){
    return _timeMgr.addTimer(delaySec,std::move(cb));
}

TimerId EventLoop::addPeriodicTimer(int delaySec, int intervalSec, TimerCallback &&cb){
    return _timeMgr.addPeriodicTimer(delaySec, intervalSec, std::move(cb));
}

void EventLoop::removeTimer(TimerId timerId){
    _timeMgr.removeTimer(timerId);
}