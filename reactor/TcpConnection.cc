#include "TcpConnection.h"
#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::ostringstream;


TcpConnection::TcpConnection(int fd,EventLoop *loop)
:_loop(loop)
,_sockIO(fd)
,_sock(fd)
,_localAddr(getLocalAddr())
,_peerAddr(getPeerAddr()){

}

TcpConnection::~TcpConnection(){

}

void TcpConnection::send(const string &msg){
    if (_sendBuffer.empty() && !_isWriting) {
        int written = _sockIO.writen(msg.c_str(), msg.size());
        if (written < (int)msg.size()) {
            // 没写完，缓存剩余部分
            _sendBuffer = msg.substr(written);
            _isWriting = true;
            _loop->addEpollWriteFd(getFd());
        }
    } else {
        // 缓冲区有数据，直接追加
        _sendBuffer += msg;
        if (!_isWriting) {
            _isWriting = true;
            _loop->addEpollWriteFd(getFd());
        }
    }
}

void TcpConnection::sendInLoop(const string &msg){
    if(_loop){
        auto self = shared_from_this();
        _loop->runInLoop([self, msg](){
            self->send(msg);
        });
    }
}

string TcpConnection::recive(){
    char buff[1024*1024] = {0};
    _sockIO.readLine(buff,sizeof(buff));

    return string(buff);
}
string TcpConnection::reciveRtspRequest(){
    _sock.setNoblock();
    char temp[4096];
    int n = ::recv(_sock.fd(), temp, sizeof(temp), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞下无数据可读，直接返回空
            return "";
        } else {
            // 其他错误
            perror("recv");
            return "";
        }
    } else if (n == 0) {
        // 对端关闭
        return "";
    }
    _recvBuffer.append(temp, n);//每次从 socket 读取数据，append 到 _recvBuffer。

    // 查找所有完整的 RTSP 请求
    size_t pos = _recvBuffer.find("\r\n\r\n");//循环查找消息分隔符（RTSP 协议用 \r\n\r\n 作为 header 结束标志）。
    if (pos != std::string::npos) {
        std::string oneRequest = _recvBuffer.substr(0, pos + 4);
        _recvBuffer.erase(0, pos + 4);
        return oneRequest;//每找到一个分隔符，就说明有一条完整的 RTSP 消息，把它从 _recvBuffer 里取出来，交给上层处理。
        //剩下的数据（可能是不完整的下一条消息），继续留在 _recvBuffer，等待下次数据到来再拼接。
    }
    // 没有完整请求，返回空
    return "";
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

void TcpConnection::setRtspConnect(std::shared_ptr<RtspConnect> conn) { _rtspConn = conn; }
std::shared_ptr<RtspConnect> TcpConnection::getRtspConnect() { return _rtspConn; }

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

TimerId TcpConnection::addOneTimer(int delaySec, TimerCallback &&cb){
    return _loop->addOneTimer(delaySec, std::move(cb));
}
TimerId TcpConnection::addPeriodicTimer(int delaySec, int intervalSec, TimerCallback &&cb){
    return _loop->addPeriodicTimer(delaySec, intervalSec, std::move(cb));
}
void TcpConnection::removeTimer(TimerId timerId){
    _loop->removeTimer(timerId);
}

void TcpConnection::handleWriteCallback() {
    if (_sendBuffer.empty()) {
        _isWriting = false;
        _loop->delEpollWriteFd(getFd());
        return;
    }
    int written = _sockIO.writen(_sendBuffer.c_str(), _sendBuffer.size());
    if (written < 0) {
        // 错误，关闭连接
        handleCloseCallback();
        return;
    }
    _sendBuffer.erase(0, written);
    if (_sendBuffer.empty()) {
        _isWriting = false;
        _loop->delEpollWriteFd(getFd());
    }
}