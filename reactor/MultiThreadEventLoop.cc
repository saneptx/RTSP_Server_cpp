#include "MultiThreadEventLoop.h"
#include "../media/RtspConnect.h"
#include <iostream>
#include <algorithm>
#include "cpp11_compat.h"

using std::cout;
using std::endl;



MultiThreadEventLoop::MultiThreadEventLoop(const std::string& ip, unsigned short port, size_t threadNum)
: _acceptor(ip, port)
, _mainLoop(_acceptor, true)
, _threadNum(threadNum)
, _threadPool(_threadNum, 10)
, _nextLoopIndex(0)
, _running(false) {
    
    // 创建子EventLoop
    for (size_t i = 0; i < _threadNum; ++i) {
        _subLoops.emplace_back(std::make_unique<EventLoop>(_acceptor, false));
    }
}

MultiThreadEventLoop::~MultiThreadEventLoop() {
    stop();
}

void MultiThreadEventLoop::start() {
    _running = true;
    
    // 启动工作线程
    // for (size_t i = 0; i < _threadNum; ++i) {
    //     _threads.emplace_back(&MultiThreadEventLoop::threadFunc, this);
    // }
    for (size_t i = 0; i < _threadNum; ++i) {
        _threadPool.addTask(std::bind(&MultiThreadEventLoop::threadFunc, this));
    }
    _threadPool.start();

    // 设置主EventLoop的回调
    _mainLoop.setNewConnectionCallback(
        std::bind(&MultiThreadEventLoop::onNewConnection, this, std::placeholders::_1));
    
    // 启动主EventLoop
    _acceptor.ready();
    _mainLoop.loop();
}

void MultiThreadEventLoop::stop() {
    if (!_running) return;
    
    _running = false;
    
    // 停止主EventLoop
    _mainLoop.unloop();
    
    // 停止所有子EventLoop
    for (auto& loop : _subLoops) {
        loop->unloop();
    }
    
    // 等待所有工作线程结束
    // for (auto& thread : _threads) {
    //     if (thread.joinable()) {
    //         thread.join();
    //     }
    // }

    // _threads.clear();
    _threadPool.stop();
    _subLoops.clear();
}

EventLoop* MultiThreadEventLoop::getNextLoop() {
    if (_subLoops.empty()) {
        return &_mainLoop;
    }
    
    // 轮询方式选择下一个EventLoop
    size_t index = _nextLoopIndex.fetch_add(1) % _subLoops.size();
    return _subLoops[index].get();
}

void MultiThreadEventLoop::threadFunc() {
    // 每个工作线程运行一个EventLoop
    size_t index = _nextLoopIndex.fetch_add(1) % _subLoops.size();
    _subLoops[index]->loop();
}

void MultiThreadEventLoop::onNewConnection(const TcpConnectionPtr& connPtr) {
    cout << connPtr->toString() << " has connected!" << endl;
    // 将新连接分配给一个子EventLoop
    EventLoop* loop = getNextLoop();
    // 在选定的EventLoop中创建RtspConnect
    loop->runInLoop([connPtr, loop, this]() {
        // 子线程 EventLoop 负责 addConnection
        loop->addConnection(connPtr);
        // 设置回调
        connPtr->setMessageCallback(
            std::bind(&MultiThreadEventLoop::onMessage, this, std::placeholders::_1));
        connPtr->setCloseCallback(
            std::bind(&MultiThreadEventLoop::onClose, this, std::placeholders::_1));
        // 创建 RtspConnect
        auto rtspConn = std::make_shared<RtspConnect>(connPtr, std::shared_ptr<EventLoop>(loop));
        connPtr->setRtspConnect(rtspConn);
    });
}

void MultiThreadEventLoop::onMessage(const TcpConnectionPtr& connPtr) {
    auto rtspConn = connPtr->getRtspConnect();
    if (rtspConn) {
        cout<<"handleRtspConnect"<<endl;
        rtspConn->handleRtspConnect();
    }
}

void MultiThreadEventLoop::onClose(const TcpConnectionPtr& connPtr) {
    cout << connPtr->toString() << " has closed!" << endl;
    auto rtspConn = connPtr->getRtspConnect();
    if (rtspConn) {
        rtspConn->releaseSession();
    }
} 