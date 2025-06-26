#ifndef __TCPSERVER_H__
#define __TCPSERVER_H__

#include "Acceptor.h"
#include "EventLoop.h"
#include "ThreadPool.h"
#include "TcpConnection.h"
#include "../media/RtspConnect.h"

class TcpServer{
public:
    TcpServer(const string &ip,unsigned short port,size_t thread_num,size_t queue_size);
    ~TcpServer();

    void start();
    void stop();

    void onNewConnection(const TcpConnectionPtr &connPtr);
    void onMessage(const TcpConnectionPtr &connPtr);
    void onClose(const TcpConnectionPtr &connPtr);


private:
    Acceptor _acceptor;
    EventLoop _loop;
    ThreadPool _pool;
};

#endif