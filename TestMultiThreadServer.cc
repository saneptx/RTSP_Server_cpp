#include "reactor/MultiThreadTcpServer.h"
#include <iostream>
#include <signal.h>
#include "reactor/cpp11_compat.h"

using std::cout;
using std::endl;
using std::cerr;

std::unique_ptr<MultiThreadTcpServer> g_server;

void signalHandler(int sig) {
    cout << "Received signal " << sig << ", shutting down server..." << endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main() {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    cout << "Starting Multi-Thread RTSP Server..." << endl;
    cout << "Event Loop Threads: 4" << endl;
    
    // 创建多线程服务器
    // 参数：IP, 端口, EventLoop线程数, 工作线程数, 队列大小
    g_server = std::make_unique<MultiThreadTcpServer>("0.0.0.0", 8888, 4);
    
    try {
        g_server->start();
    } catch (const std::exception& e) {
        cerr << "Server error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
} 