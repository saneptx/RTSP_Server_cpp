#include "Eventor.h"
#include "Logger.h"
#include <string.h>

Eventor::Eventor()
:_evtfd(createEventFd()){
    LOG_DEBUG("Eventor created with fd: %d", _evtfd);
}

Eventor::~Eventor(){
    LOG_DEBUG("Eventor destructor, closing fd: %d", _evtfd);
    close(_evtfd);
}

void Eventor::wakeUp(){
    uint64_t one = 1;
    ssize_t ret = write(_evtfd,&one,sizeof(uint64_t));
    if(ret != sizeof(uint64_t)){
        LOG_ERROR("Eventor wakeUp write failed: %s", strerror(errno));
        return;
    }
    LOG_DEBUG("Eventor wakeUp successful");
}

int Eventor::getEvtfd(){
    return _evtfd;
}

void Eventor::handleRead(){
    uint64_t two;
    ssize_t ret = read(_evtfd,&two,sizeof(uint64_t));
    if(ret != sizeof(uint64_t)){
        LOG_ERROR("Eventor handleRead failed: %s", strerror(errno));
        return;
    }
    LOG_DEBUG("Eventor handleRead successful, processing %zu pending functions", _pendings.size());
    doPenddingFunctors();
}

void Eventor::addEventcb(Functor &&cb){
    std::unique_lock<std::mutex> autoLock(_mutex);
    _pendings.push_back(std::move(cb));
    LOG_DEBUG("Added event callback, total pending: %zu", _pendings.size());
    wakeUp();
}

void Eventor::doPenddingFunctors(){
    std::vector<Functor> tmp;
    std::unique_lock<std::mutex> autoLock(_mutex);
    tmp.swap(_pendings);
    autoLock.unlock();//手动提前解锁
    LOG_DEBUG("Executing %zu pending functions", tmp.size());
    for(auto &cb:tmp){
        cb();
    }
    LOG_DEBUG("All pending functions executed");
}

int Eventor::createEventFd(){
    int fd = eventfd(0,0);
    if(fd < 0){
        LOG_ERROR("Eventor createEventFd failed: %s", strerror(errno));
    }
    return fd;
}