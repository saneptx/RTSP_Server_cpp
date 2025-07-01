#include "MultiThreadTcpServer.h"
#include <iostream>
#include "cpp11_compat.h"

using std::cout;
using std::endl;

MultiThreadTcpServer::MultiThreadTcpServer(
const std::string& ip
, unsigned short port
, size_t eventLoopThreadNum
// , size_t workerThreadNum
// , size_t queueSize
)
: _eventLoopThreadNum(eventLoopThreadNum)
// , _workerThreadNum(workerThreadNum)
// , _queueSize(queueSize) 
{
    
    _eventLoopManager = std::make_unique<MultiThreadEventLoop>(ip, port, eventLoopThreadNum);
    // _workerPool = std::make_shared<ThreadPool>(workerThreadNum, queueSize);
}

MultiThreadTcpServer::~MultiThreadTcpServer() {
    stop();
}

void MultiThreadTcpServer::start() {
    cout << "Starting MultiThreadTcpServer with " << _eventLoopThreadNum 
         << " event loop threads and "
        //  << _workerThreadNum << " worker threads" 
         << endl;
    
    // 启动工作线程池
    // _workerPool->start();
    
    // 启动事件循环管理器
    _eventLoopManager->start();
}

void MultiThreadTcpServer::stop() {
    cout << "Stopping MultiThreadTcpServer..." << endl;
    
    if (_eventLoopManager) {
        _eventLoopManager->stop();
    }
    
    // if (_workerPool) {
    //     _workerPool->stop();
    // }
} 