#include "InetAddress.h"
#include <strings.h>
InetAddress::InetAddress(){
    
}
InetAddress::InetAddress(const string &ip,unsigned short port)
{
    ::bzero(&_addr,sizeof(struct sockaddr_in));
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(port);
    _addr.sin_addr.s_addr = inet_addr(ip.c_str());//host->network
}
InetAddress::InetAddress(const struct sockaddr_in &addr)
:_addr(addr){
    
}
InetAddress::~InetAddress(){

}
string InetAddress::ip() const{
    return string(inet_ntoa(_addr.sin_addr));//network->host
}
unsigned short InetAddress::port() const{
    return ntohs(_addr.sin_port);//network->host
}
const struct sockaddr_in *InetAddress::getInetAddrPtr() const{
    return &_addr;
}
string InetAddress::toString(){
    return ip() + ":" + std::to_string(port());
}