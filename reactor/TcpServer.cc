#include "TcpServer.h"
#include "TcpConnection.h"

MyTask::MyTask(TcpConnectionPtr connPtr)
:_connPtr(connPtr)
,_rtspConn(_connPtr){

}
void MyTask::process(){
    _rtspConn.handleRtspConnect();
}

TcpServer::TcpServer(const string &ip,unsigned short port,size_t thread_num,size_t queue_size)
:_acceptor(ip,port)
,_loop(_acceptor)
,_pool(thread_num,queue_size){

}
TcpServer::~TcpServer(){

}

void TcpServer::start(){
    _pool.start();//注意要先启动线程池，否则会阻塞在_loop.loop()函数
    _loop.setNewConnectionCallback(std::bind(&TcpServer::onNewConnection,this,std::placeholders::_1));
    _loop.setMessageCallback(std::bind(&TcpServer::onMessage,this,std::placeholders::_1));
    _loop.setCloseCallback(std::bind(&TcpServer::onClose,this,std::placeholders::_1));
    _acceptor.ready();
    _loop.loop();
}
void TcpServer::stop(){
    _loop.unloop();
    _pool.stop();
}

void TcpServer::onNewConnection(const TcpConnectionPtr &connPtr){
    cout << connPtr->toString() << " has connected!"<< endl;
}
void TcpServer::onMessage(const TcpConnectionPtr &connPtr){
    // string msg = connPtr->recive();
    // cout<<">>recv client msg = "<< msg;
    MyTask task(connPtr);
    _pool.addTask(std::bind(&MyTask::process,task));
}
void TcpServer::onClose(const TcpConnectionPtr &connPtr){
    cout << connPtr->toString() << " has closed!"<< endl;
}