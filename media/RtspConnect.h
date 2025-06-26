#ifndef __RTSPCONNECT_H__
#define __RTSPCONNECT_H__
#include "../reactor/TcpConnection.h"
#include <string>
#include <unordered_map>
#include <mutex>
using std::string;
using std::mutex;
using std::unordered_map;


struct RtspSession {
    std::string sessionId;
    bool isPlaying = false;
    string clientIP;
    time_t lastActive = time(nullptr);
};
class RtspConnect{
public:
    RtspConnect(TcpConnectionPtr connPtr);
    ~RtspConnect();
    void handleRtspConnect();
    void releaseSession();
private:
    void parseRequest(const std::string& rBuf);
    void handleOptions();
    void handleDescribe();
    void handleSetup();
    void handlePlay();
    void handleTeardown();
    void sendResponse(const std::string& response);
    string generateSessionId();
    TcpConnectionPtr _connPtr;
    string method,url,version;
    int CSeq;
    string transport;
    // sessionID -> Session 映射
    // _sessionMap 和 _sessionMutex 是静态的，因为多个 RtspConnect 实例要共享会话池。
    static unordered_map<std::string, RtspSession> _sessionMap;
    static mutex _sessionMutex;
    string currentSessionId;
};


#endif