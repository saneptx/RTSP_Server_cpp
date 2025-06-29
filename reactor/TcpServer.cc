#include "TcpServer.h"
#include "TcpConnection.h"


TcpServer::TcpServer(const string &ip,unsigned short port,size_t thread_num,size_t queue_size)
:_acceptor(ip,port)
,_loop(_acceptor)
,_pool(std::make_shared<ThreadPool>(thread_num, queue_size)){

}
TcpServer::~TcpServer(){

}

void TcpServer::start(){
    _pool->start();//注意要先启动线程池，否则会阻塞在_loop.loop()函数
    _loop.setNewConnectionCallback(std::bind(&TcpServer::onNewConnection,this,std::placeholders::_1));
    _loop.setMessageCallback(std::bind(&TcpServer::onMessage,this,std::placeholders::_1));
    _loop.setCloseCallback(std::bind(&TcpServer::onClose,this,std::placeholders::_1));
    _acceptor.ready();
    _loop.loop();
}
void TcpServer::stop(){
    _loop.unloop();
    _pool->stop();
}

void TcpServer::onNewConnection(const TcpConnectionPtr &connPtr){
    cout << connPtr->toString() << " has connected!"<< endl;
    auto rtspConn = std::make_shared<RtspConnect>(connPtr);
    connPtr->setRtspConnect(rtspConn);
}
void TcpServer::onMessage(const TcpConnectionPtr &connPtr){
    auto rtspConn = connPtr->getRtspConnect();
    if (rtspConn) {
        rtspConn->handleRtspConnect();
    }
}
void TcpServer::onClose(const TcpConnectionPtr &connPtr){
    cout << connPtr->toString() << " has closed!"<< endl;
    auto rtspConn = connPtr->getRtspConnect();
    if (rtspConn) {
        rtspConn->releaseSession();
    }
}