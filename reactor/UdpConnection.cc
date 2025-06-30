#include "UdpConnection.h"
#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::ostringstream;

UdpConnection::UdpConnection(const string &ip,unsigned short port, std::shared_ptr<EventLoop> loopPtr)
    : _sock(ip,port), _loopPtr(loopPtr), _localAddr(getLocalAddr()) {
}

UdpConnection::~UdpConnection() {
}

void UdpConnection::send(const std::string& msg) {
    _sock.sendto(msg.c_str(), msg.size());
}

void UdpConnection::sendInLoop(const std::string& msg) {
    if (_loopPtr) {
        _loopPtr->runInLoop(std::move([this, msg]() {
            this->send(msg);
        }));
    }
}

std::string UdpConnection::recv() {
    char buff[4096] = {0};
    int n = _sock.recvfrom(buff, sizeof(buff));
    _peerAddr = _sock.getPeerAddr();
    if (n > 0) {
        return std::string(buff, n);
    }
    return "";
}

void UdpConnection::setMessageCallback(const UdpConnectionCallback& cb) {
    _onMessageCb = cb;
}

void UdpConnection::handleMessageCallback() {
    if (_onMessageCb) {
        _onMessageCb(shared_from_this());
    }
}

InetAddress UdpConnection::getLocalAddr() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr);
    int ret = getsockname(_sock.fd(), (struct sockaddr*)&addr, &len);
    if (-1 == ret) {
        perror("getsockname");
    }
    return InetAddress(addr);
}

InetAddress UdpConnection::getPeerAddr(){
    return _peerAddr;
}

int UdpConnection::getUdpFd() const {
    return _sock.fd();
}

TimerId UdpConnection::addOneTimer(int delaySec, TimerCallback&& cb) {
    return _loopPtr->addOneTimer(delaySec, std::move(cb));
}

TimerId UdpConnection::addPeriodicTimer(int delaySec, int intervalSec, TimerCallback&& cb) {
    return _loopPtr->addPeriodicTimer(delaySec, intervalSec, std::move(cb));
}

void UdpConnection::removeTimer(TimerId timerId) {
    _loopPtr->removeTimer(timerId);
} 