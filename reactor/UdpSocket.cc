#include "UdpSocket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

UdpSocket::UdpSocket(const string &ip,unsigned short port,InetAddress clientAddr)
:_serverAddr(ip,port)
,_clientAddr(clientAddr){
    _fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (_fd < 0) {
        perror("socket");
        return;
    }
    bind();
    setNoblock();
}

UdpSocket::UdpSocket(int fd) : _fd(fd) {
}

UdpSocket::~UdpSocket() {
    if (_fd >= 0) {
        ::close(_fd);
    }
}

int UdpSocket::fd() const {
    return _fd;
}

void UdpSocket::setNoblock() {
    int flags = fcntl(_fd, F_GETFL, 0);
    fcntl(_fd, F_SETFL, flags | O_NONBLOCK);
}

void UdpSocket::setReuseAddr(){
    int on = 1;
    int ret = setsockopt(_fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    if(ret){
        perror("setsocketopt");
        return;
    }
}
void UdpSocket::setReusePort(){
    int on = 1;
    int ret = setsockopt(_fd,SOL_SOCKET,SO_REUSEPORT,&on,sizeof(on));
    if(ret){
        perror("setsocketopt");
        return;
    }
}

int UdpSocket::bind() {
    int ret = ::bind(_fd, (struct sockaddr *)_serverAddr.getInetAddrPtr(), sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("bind");
    }
    return ret;
}

int UdpSocket::sendto(const void* data, size_t len) {
    int ret = ::sendto(_fd, data, len, 0, (struct sockaddr *)_clientAddr.getInetAddrPtr(), sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("sendto");
    }
    return ret;
}

int UdpSocket::recvfrom(void* data, size_t len) {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    
    int ret = ::recvfrom(_fd, data, len, 0, (struct sockaddr*)&_clientAddr, &addrLen);
    if (ret == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
    } else {
        _clientAddr = InetAddress(clientAddr);
    }
    return ret;
} 


InetAddress UdpSocket::getPeerAddr(){
    return _clientAddr;
} 