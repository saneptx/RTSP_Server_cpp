#ifndef __RTSPCONNECT_H__
#define __RTSPCONNECT_H__
#include "../reactor/TcpConnection.h"
#include "../reactor/UdpConnection.h"
#include "../reactor/EventLoop.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include "RtpPusher.h"
#include "H264FileReader.h"
#include "AacFileReader.h"
using std::string;
using std::mutex;
using std::unordered_map;

using EventLoopPtr = std::shared_ptr<EventLoop>;
struct RtspSession {
    std::string sessionId;
    bool isPlaying = false;
    string clientIP;
    time_t lastActive = time(nullptr);
    
    // UDP传输相关
    bool useUdp = false;
    InetAddress clientVideoRtpAddr;  // 客户端传输视频RTP包的UDP地址,RTCP为Port+1
    InetAddress clientVideoRtcpAddr;  // 客户端传输视频RTP包的UDP地址,RTCP为Port+1
    InetAddress clientAudioRtpAddr;  // 客户端传输音频RTP包的UDP地址,RTCP为Port+1
    InetAddress clientAudioRtcpAddr;  // 客户端传输音频RTP包的UDP地址,RTCP为Port+1
    int serverVideoPort = 0;      // 服务器传输视频RTP端口,RTCP为Port+1
    int serverAudioPort = 0;      // 服务器传输音频RTP端口,RTCP为Port+1
};
class RtspConnect{
public:
    RtspConnect(TcpConnectionPtr connPtr,EventLoopPtr loopPtr);
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
    int allocateUdpPorts();  // 分配UDP端口
    void releaseUdpPorts();  // 释放UDP端口
    
    TcpConnectionPtr _connPtr;
    EventLoopPtr _loopPtr;

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
    
    // UDP连接
    std::shared_ptr<UdpConnection> _videoRtpConn;
    std::shared_ptr<UdpConnection> _videoRtcpConn;
    std::shared_ptr<UdpConnection> _audioRtpConn;
    std::shared_ptr<UdpConnection> _audioRtcpConn;
};


#endif