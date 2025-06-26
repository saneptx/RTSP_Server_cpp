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

class RtspConnect;
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
    string reciveRtspRequest();//接收Rtsp请求
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

    InetAddress getLocalAddr();
    InetAddress getPeerAddr();

    void setRtspConnect(std::shared_ptr<RtspConnect> conn);
    std::shared_ptr<RtspConnect> getRtspConnect();
private:
    EventLoop *_loop;
    SocketIO _sockIO;
    Socket _sock;
    InetAddress _localAddr;
    InetAddress _peerAddr;
    /*
    为什么要有持久化 buffer？
    你每次 recv 到的数据，可能只是“消息的一部分”，也可能是“多条消息”。
    如果你只处理本次 recv 的内容，就会丢失消息边界，导致解析出错。
    持久化 buffer就是把每次 recv 到的数据都 append 到一个成员变量（如 _recvBuffer）里，只要没处理完的数据都留着，直到拼出完整的消息。
    */
    std::string _recvBuffer;//持久化buffer

    std::shared_ptr<RtspConnect> _rtspConn;
    TcpConnectionCallback _onNewConnectionCb;
    TcpConnectionCallback _onMessageCb;
    TcpConnectionCallback _onCloseCb;
};


#endif