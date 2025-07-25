#include "Socket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

Socket::Socket(){
    _fd = ::socket(AF_INET,SOCK_STREAM,0);
    if(_fd < 0){
        perror("socket");
        return;
    }
    setNoblock();
}
Socket::Socket(int fd)
:_fd(fd){

}
Socket::~Socket(){

}
int Socket::fd() const{
    return _fd;
}
void Socket::setNoblock(){
    int flags = fcntl(_fd, F_GETFL, 0);
    fcntl(_fd, F_SETFL, flags | O_NONBLOCK);
}
