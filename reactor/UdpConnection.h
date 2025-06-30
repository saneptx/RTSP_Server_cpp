#ifndef __UDPCONNECTION_H__
#define __UDPCONNECTION_H__

#include "UdpSocket.h"
#include "InetAddress.h"
#include "EventLoop.h"
#include <memory>
#include <functional>
#include <string>

using std::shared_ptr;
using std::function;
using TimerCallback = std::function<void()>;
using TimerId = uint64_t; 

class EventLoop;
class UdpConnection : public std::enable_shared_from_this<UdpConnection> {
    using UdpConnectionPtr = shared_ptr<UdpConnection>;
    using UdpConnectionCallback = function<void(const UdpConnectionPtr&)>;
    
public:
    explicit UdpConnection(const string &ip,unsigned short port, std::shared_ptr<EventLoop> loopPtr);
    ~UdpConnection();
    
    void send(const std::string& msg);
    void sendInLoop(const std::string& msg);
    std::string recv();
    
    // 回调函数注册
    void setMessageCallback(const UdpConnectionCallback& cb);
    
    // 回调函数执行
    void handleMessageCallback();
    
    InetAddress getLocalAddr();
    InetAddress getPeerAddr();
    
    int getUdpFd() const;
    
    TimerId addOneTimer(int delaySec, TimerCallback&& cb);
    TimerId addPeriodicTimer(int delaySec, int intervalSec, TimerCallback&& cb);
    void removeTimer(TimerId timerId);
    
private:
    std::shared_ptr<EventLoop> _loopPtr;
    UdpSocket _sock;
    InetAddress _localAddr;
    InetAddress _peerAddr;
    
    UdpConnectionCallback _onMessageCb;
};

#endif 