#include "Acceptor.h"
#include "Logger.h"
#include <string.h>

Acceptor::Acceptor(const string &ip,unsigned short port)
:_sock()
,_addr(ip,port){
    LOG_INFO("Acceptor created - IP: %s, Port: %d", ip.c_str(), port);
}

Acceptor::~Acceptor(){
    LOG_DEBUG("Acceptor destructor called");
}

void Acceptor::ready(){
    LOG_INFO("Preparing Acceptor for listening...");
    setReuseAddr();
    setReusePort();
    bind();
    listen();
    LOG_INFO("Acceptor ready for accepting connections");
}

void Acceptor::setReuseAddr(){
    int on = 1;
    int ret = setsockopt(_sock.fd(),SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    if(ret){
        LOG_ERROR("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        return;
    }
    LOG_DEBUG("SO_REUSEADDR set successfully");
}

void Acceptor::setReusePort(){
    int on = 1;
    int ret = setsockopt(_sock.fd(),SOL_SOCKET,SO_REUSEPORT,&on,sizeof(on));
    if(ret){
        LOG_ERROR("setsockopt SO_REUSEPORT failed: %s", strerror(errno));
        return;
    }
    LOG_DEBUG("SO_REUSEPORT set successfully");
}

void Acceptor::bind(){
    int ret = ::bind(_sock.fd(), (struct sockaddr *)_addr.getInetAddrPtr(), sizeof(struct sockaddr));
    if(ret == -1){
        LOG_ERROR("bind failed: %s", strerror(errno));
        return;
    }
    LOG_INFO("Socket bound to %s:%d", _addr.ip().c_str(), _addr.port());
}

void Acceptor::listen(){
    int ret = ::listen(_sock.fd(),128);
    if(-1 == ret){
        LOG_ERROR("listen failed: %s", strerror(errno));
        return;
    }
    LOG_INFO("Socket listening with backlog: 128");
}

int Acceptor::accept(){
    int connfd = ::accept(_sock.fd(),nullptr,nullptr);
    if(-1 == connfd){
        LOG_ERROR("accept failed: %s", strerror(errno));
        return -1;
    }
    LOG_DEBUG("Accepted new connection, fd: %d", connfd);
    return connfd;
}

int Acceptor::fd() const{
    return _sock.fd();
}