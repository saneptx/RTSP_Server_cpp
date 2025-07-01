#ifndef __MULTITHREADEVENTLOOP_H__
#define __MULTITHREADEVENTLOOP_H__

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "EventLoop.h"
#include "Acceptor.h"
#include "TcpConnection.h"
#include "ThreadPool.h"

class MultiThreadEventLoop {
public:
    MultiThreadEventLoop(const std::string& ip, unsigned short port, size_t threadNum);
    ~MultiThreadEventLoop();

    void start();
    void stop();

    // 获取下一个EventLoop（轮询方式）
    EventLoop* getNextLoop();
    
    // 获取主EventLoop
    EventLoop* getMainLoop() { return &_mainLoop; }

private:
    void threadFunc();  // 工作线程函数
    void onNewConnection(const TcpConnectionPtr& connPtr);
    void onMessage(const TcpConnectionPtr& connPtr);
    void onClose(const TcpConnectionPtr& connPtr);

private:
    Acceptor _acceptor;
    EventLoop _mainLoop;  // 主线程的EventLoop，负责接受连接
    
    std::vector<std::unique_ptr<EventLoop>> _subLoops;  // 子线程的EventLoop
    size_t _threadNum;

    // std::vector<std::thread> _threads;  // 工作线程
    ThreadPool _threadPool;
    
    std::atomic<size_t> _nextLoopIndex;  // 下一个要使用的EventLoop索引
    
    std::atomic<bool> _running;
};

#endif 