#ifndef __UDPSOCKET_H__
#define __UDPSOCKET_H__

#include "NonCopyable.h"
#include "InetAddress.h"
#include <sys/socket.h>

class UdpSocket : NonCopyable {
public:
    UdpSocket(const string &ip,unsigned short port,InetAddress clientAddr);
    explicit UdpSocket(int fd);
    ~UdpSocket();
    
    int fd() const;
    void setNoblock();
    void setReuseAddr();
    void setReusePort();
    // UDP特有方法
    int bind();
    int sendto(const void* data, size_t len);
    int recvfrom(void* data, size_t len);
    void setPeerAddr(InetAddress clientAddr);
    void closeUdp();
    InetAddress getPeerAddr();
    
private:
    int _fd;
    InetAddress _serverAddr;//服务器地址
    InetAddress _clientAddr;//客户端地址
};

#endif 