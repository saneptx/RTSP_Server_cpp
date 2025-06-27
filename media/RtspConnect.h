#ifndef __RTSPCONNECT_H__
#define __RTSPCONNECT_H__
#include "../reactor/TcpConnection.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include "../reactor/ThreadPool.h"
#include "RtpPusher.h"
#include "H264FileReader.h"
#include "AacFileReader.h"
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
    RtspConnect(TcpConnectionPtr connPtr,std::shared_ptr<ThreadPool> pool);
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
    std::shared_ptr<ThreadPool> _pool;
    string method,url,version;
    int CSeq;
    string transport;
    // sessionID -> Session 映射
    // _sessionMap 和 _sessionMutex 是静态的，因为多个 RtspConnect 实例要共享会话池。
    static unordered_map<std::string, RtspSession> _sessionMap;
    static mutex _sessionMutex;
    string currentSessionId;
    std::shared_ptr<H264FileReader> _h264FileReaderPtr;
    std::shared_ptr<AacFileReader> _aacFileReaderPtr;
    RtpPusher _rtspPusher;
};


#endif