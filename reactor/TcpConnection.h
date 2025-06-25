#ifndef __TCPCONNECTION_H__
#define __TCPCONNECTION_H__

#include "Socket.h"
#include "SocketIO.h"
#include "InetAddress.h"
#include "EventLoop.h"
#include <memory>
#include <functional>
#include <string>
using std::shared_ptr;
using std::function;


class EventLoop;
class TcpConnection
:public std::enable_shared_from_this<TcpConnection>{
    using TcpConnectionPtr = shared_ptr<TcpConnection>;
    using TcpConnectionCallback = function<void(const TcpConnectionPtr &)>;
public:
    explicit TcpConnection(int fd,EventLoop *loop);
    ~TcpConnection();
    void send(const string &msg);
    void sendInLoop(const string &msg);
    string recive();
    string reciveRtspRequest();
    string toString();
    bool isClosed() const;


    //回调函数注册
    void setNewConnectionCallback(const TcpConnectionCallback &cb);
    void setMessageCallback(const TcpConnectionCallback &cb);
    void setCloseCallback(const TcpConnectionCallback &cb);

    //回调函数执行
    void handleNewConnectionCallback();
    void handleMessageCallback();
    void handleCloseCallback();
private:
    InetAddress getLocalAddr();
    InetAddress getPeerAddr();
private:
    EventLoop *_loop;
    SocketIO _sockIO;
    Socket _sock;
    InetAddress _localAddr;
    InetAddress _peerAddr;

    TcpConnectionCallback _onNewConnectionCb;
    TcpConnectionCallback _onMessageCb;
    TcpConnectionCallback _onCloseCb;
};


#endif