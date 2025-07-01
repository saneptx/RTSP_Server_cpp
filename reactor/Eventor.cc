#include "Eventor.h"


Eventor::Eventor()
:_evtfd(createEventFd()){
    
}
Eventor::~Eventor(){
    close(_evtfd);
}
void Eventor::wakeUp(){
    uint64_t one = 1;
    ssize_t ret = write(_evtfd,&one,sizeof(uint64_t));
    if(ret != sizeof(uint64_t)){
        perror("write");
        return;
    }
}
int Eventor::getEvtfd(){
    return _evtfd;
}
void Eventor::handleRead(){
    uint64_t two;
    ssize_t ret = read(_evtfd,&two,sizeof(uint64_t));
    if(ret != sizeof(uint64_t)){
        perror("read");
        return;
    }
    doPenddingFunctors();
}
void Eventor::addEventcb(Functor &&cb){
    std::unique_lock<std::mutex> autoLock(_mutex);
    _pendings.push_back(std::move(cb));
    wakeUp();
}

void Eventor::doPenddingFunctors(){
    std::vector<Functor> tmp;
    std::unique_lock<std::mutex> autoLock(_mutex);
    tmp.swap(_pendings);
    autoLock.unlock();//手动提前解锁
    for(auto &cb:tmp){
        cb();
    }
}
int Eventor::createEventFd(){
    int fd = eventfd(0,0);
    if(fd < 0){
        perror("Eventor");
    }
    return fd;
}