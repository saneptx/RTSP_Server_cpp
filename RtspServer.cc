#include "reactor/MultiThreadEventLoop.h"
#include <iostream>
#include <signal.h>
#include "reactor/cpp11_compat.h"
#include "reactor/Logger.h"

using std::cout;
using std::endl;
using std::cerr;

std::unique_ptr<MultiThreadEventLoop> g_server;

void signalHandler(int sig) {
    LOG_INFO("Received signal %d, shutting down server...",sig);
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main() {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    LOG_INFO("Starting Multi-Thread RTSP Server...");
    LOG_INFO("Event Loop Threads: 4");
    
    // 创建多线程服务器
    // 参数：IP, 端口, EventLoop线程数, 工作线程数, 队列大小
    g_server = std::make_unique<MultiThreadEventLoop>("0.0.0.0", 8888, 4);
    
    try {
        g_server->start();
    } catch (const std::exception& e) {
        LOG_ERROR("Server error: %s",e.what());
        return 1;
    } catch (...) {
    LOG_ERROR("Unknown exception caught in main!");
        return 2;
    }
    
    return 0;
} 