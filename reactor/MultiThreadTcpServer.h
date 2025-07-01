#ifndef __MULTITHREADTCPSERVER_H__
#define __MULTITHREADTCPSERVER_H__

#include "MultiThreadEventLoop.h"
#include "ThreadPool.h"
#include <memory>

class MultiThreadTcpServer {
public:
    MultiThreadTcpServer(const std::string& ip, unsigned short port, 
                        size_t eventLoopThreadNum
                        // , size_t workerThreadNum
                        // , size_t queueSize
                        );
    ~MultiThreadTcpServer();

    void start();
    void stop();

private:
    std::unique_ptr<MultiThreadEventLoop> _eventLoopManager;
    // std::shared_ptr<ThreadPool> _workerPool;  // 用于处理业务逻辑的工作线程池
    
    size_t _eventLoopThreadNum;
    // size_t _workerThreadNum;
    // size_t _queueSize;
};

#endif 