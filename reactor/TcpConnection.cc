#include "TcpConnection.h"
#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::ostringstream;


TcpConnection::TcpConnection(int fd,EventLoop *loop)
:_sockIO(fd)
,_sock(fd)
,_loop(loop)
,_localAddr(getLocalAddr())
,_peerAddr(getPeerAddr()){

}

TcpConnection::~TcpConnection(){

}

void TcpConnection::send(const string &msg){
    _sockIO.writen(msg.c_str(),msg.size());
}

void TcpConnection::sendInLoop(const string &msg){
    if(_loop){
        _loop->runInLoop(std::move(bind(&TcpConnection::send,this,msg)));
    }
}

string TcpConnection::recive(){
    char buff[65535] = {0};
    _sockIO.readLine(buff,sizeof(buff));

    return string(buff);
}
string TcpConnection::reciveRtspRequest(){
    char buff[65535] = {0};
    _sockIO.readCRLFCRLF(buff,sizeof(buff));

    return string(buff);
}
string TcpConnection::toString(){
    ostringstream oss;
    oss << _localAddr.ip()<<":"
        << _localAddr.port()<<"---->"
        << _peerAddr.ip()<<":"
        << _peerAddr.port();
    return oss.str();
}

//获取本端网络地址信息
InetAddress TcpConnection::getLocalAddr(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr );
    //获取本端地址的函数getsockname
    int ret = getsockname(_sock.fd(), (struct sockaddr *)&addr, &len);
    if(-1 == ret)
    {
        perror("getsockname");
    }

    return InetAddress(addr);
}

//获取对端的网络地址信息
InetAddress TcpConnection::getPeerAddr(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr );
    //获取对端地址的函数getpeername
    int ret = getpeername(_sock.fd(), (struct sockaddr *)&addr, &len);
    if(-1 == ret)
    {
        perror("getpeername");
    }

    return InetAddress(addr);
}

bool TcpConnection::isClosed() const{
    char buf[10]={0};
    int ret = ::recv(_sock.fd(),buf,sizeof(buf),MSG_PEEK);

    return (0 == ret);
}

void TcpConnection::setNewConnectionCallback(const TcpConnectionCallback &cb){
    _onNewConnectionCb = cb;
}
void TcpConnection::setMessageCallback(const TcpConnectionCallback &cb){
    _onMessageCb = cb;
}
void TcpConnection::setCloseCallback(const TcpConnectionCallback &cb){
    _onCloseCb = cb;
}


void TcpConnection::handleNewConnectionCallback(){
    if(_onNewConnectionCb){
        _onNewConnectionCb(shared_from_this());
    }else{
        cout<<"_onNewConnectionCb == nullptr"<<endl;
    }
}
void TcpConnection::handleMessageCallback(){
    if(_onMessageCb){
        _onMessageCb(shared_from_this());
    }else{
        cout<<"_onMessageCb == nullptr"<<endl;
    }
}
void TcpConnection::handleCloseCallback(){
    if(_onCloseCb){
        _onCloseCb(shared_from_this());
    }else{
        cout<<"_onCloseCb == nullptr"<<endl;
    }
}