#include "RtspConnect.h"
#include <iostream>
#include <sstream>
#include <atomic>
#include <thread>
using std::cout;
using std::endl;

//初始化静态成员
std::unordered_map<std::string, RtspSession> RtspConnect::_sessionMap;
std::mutex RtspConnect::_sessionMutex;

RtspConnect::RtspConnect(TcpConnectionPtr connPtr,std::shared_ptr<ThreadPool> pool)
:_connPtr(connPtr)
,_pool(pool)
,method("")
,url("")
,version("")
,CSeq(0)
,transport("")
,currentSessionId("")
,_h264FileReaderPtr(std::make_shared<H264FileReader>("data/1.h264"))
,_aacFileReaderPtr(std::make_shared<AacFileReader>("data/1.aac"))
,_rtspPusher(_connPtr,_pool,_h264FileReaderPtr,_aacFileReaderPtr)
{
    std::cout << "[RtspConnect] constructed, this=" << this << std::endl;
}
RtspConnect::~RtspConnect(){
    std::cout << "[RtspConnect] destructed, this=" << this << std::endl;
}
void RtspConnect::handleRtspConnect(){

    std::string rBuf = _connPtr->reciveRtspRequest();
    if (rBuf.empty()) {
        // 没有完整请求，跳出或等待
        return;
    }
    std::cout << "recv data from client:\n" << rBuf << std::endl;

    // 1. 解析请求
    parseRequest(rBuf);

    // 2. 路由处理
    if (method == "OPTIONS") {
        handleOptions();
    } else if (method == "DESCRIBE") {
        handleDescribe();
    } else if (method == "SETUP") {
        handleSetup();
    } else if (method == "PLAY") {
        handlePlay();
    } else if (method == "TEARDOWN") {
        handleTeardown();
    } else {
        sendResponse("RTSP/1.0 400 Bad Request\r\nCSeq: " + std::to_string(CSeq) + "\r\n\r\n");
    }
    
}

void RtspConnect::releaseSession() {
    std::lock_guard<std::mutex> lock(_sessionMutex);
    if (!currentSessionId.empty()) {
        auto it = _sessionMap.find(currentSessionId);
        if (it != _sessionMap.end()) {
            _sessionMap.erase(it);
            std::cout << "Session " << currentSessionId << " released." << std::endl;
        }
        currentSessionId.clear();
    }
    _rtspPusher.stop();
}

void RtspConnect::parseRequest(const std::string& rBuf) {
    std::istringstream iss(rBuf);
    std::string line;
    std::vector<std::string> headers;
    std::string lastHeader;

    while (std::getline(iss, line)) {
        // 去掉行尾的 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // 如果是空行，header结束
        if (line.empty()) break;

        // 判断是否为折行（以空格或Tab开头）
        if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
            if (!headers.empty()) {
                headers.back() += line;
            }
        } else {
            headers.push_back(line);
        }
    }
    // 处理请求行和各个header
    for (size_t i = 0; i < headers.size(); ++i) {
        const std::string& h = headers[i];
        if (i == 0 && h.find("RTSP/") != std::string::npos) {
            std::istringstream lss(h);
            lss >> method >> url >> version;
        } else if (h.find("CSeq") != std::string::npos || h.find("CSEQ") != std::string::npos) {
            size_t pos = h.find(":");
            if (pos != std::string::npos)
                CSeq = std::stoi(h.substr(pos + 1));
        } else if (h.find("Transport") != std::string::npos) {
            transport = h;
        } else if (h.find("Session:") != std::string::npos){
            size_t pos = h.find(":");
            if (pos != std::string::npos)
                currentSessionId = h.substr(pos + 1);
            currentSessionId.erase(0, currentSessionId.find_first_not_of(" \t")); // 去空格
        }
        // 你可以继续处理其他header
    }
}
void RtspConnect::handleOptions() {
    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n"
                           "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n";
    sendResponse(response);
}
void RtspConnect::handleDescribe() {
    std::string localIP;
    size_t start = url.find("rtsp://");
    if (start != std::string::npos) {
        start += 7;
        size_t end = url.find(":", start);
        if (end != std::string::npos) {
            localIP = url.substr(start, end - start);
        }
    }
    // 示例 SDP 内容
    std::string sdp = "v=0\r\n"
                      "o=- 9" + std::to_string(time(NULL)) + " 1 IN IP4 " + localIP + "\r\n"
                      "s=Unnamed\r\n"
                      "t=0 0\r\n"
                      "a=control:*\r\n"
                      "m=video 0 RTP/AVP 96\r\n"
                      "a=rtpmap:96 H264/90000\r\n"
                      "a=control:track0\r\n"
                      "m=audio 0 RTP/AVP 97\r\n"
                      "a=rtpmap:97 MPEG4-GENERIC/44100/2\r\n"
                      "a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210;\r\n"
                      "a=control:track1\r\n\r\n";

    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n"
                           "Content-Base: rtsp://" + localIP + "/\r\n"
                           "Content-Type: application/sdp\r\n"
                           "Content-Length: " + std::to_string(sdp.size()) + "\r\n\r\n" + sdp;
    sendResponse(response);
}
void RtspConnect::handleSetup() {
    std::lock_guard<std::mutex> lock(_sessionMutex);

    // 没有 session 时，生成并创建新 session
    if (currentSessionId.empty()) {
        currentSessionId = generateSessionId();
        struct RtspSession session;
        session.sessionId = currentSessionId;
        session.clientIP = _connPtr->getPeerAddr().ip(); // 可以从socket获取
        _sessionMap[currentSessionId] = session;
    }

    RtspSession& session = _sessionMap[currentSessionId];
    session.lastActive = time(nullptr);
    std::string response;
    if (url.find("track0") != std::string::npos) { // 视频
        response = "RTSP/1.0 200 OK\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n"
                   "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                   "Session: " + currentSessionId + "\r\n\r\n";
    } else if (url.find("track1") != std::string::npos) { // 音频
        response = "RTSP/1.0 200 OK\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n"
                   "Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n"
                   "Session: " + currentSessionId + "\r\n\r\n";
    } else {
        response = "RTSP/1.0 454 Session Not Found\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n\r\n";
    }
    sendResponse(response);
}
void RtspConnect::handlePlay() {
    std::lock_guard<std::mutex> lock(_sessionMutex);
    auto it = _sessionMap.find(currentSessionId);
    if (it == _sessionMap.end()) {
        sendResponse("RTSP/1.0 454 Session Not Found\r\nCSeq: " + std::to_string(CSeq) + "\r\n\r\n");
        return;
    }

    it->second.isPlaying = true;
    it->second.lastActive = time(nullptr);
    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n"
                           "Session: " + currentSessionId + "\r\n\r\n";
    sendResponse(response);
    // _rtspPusher.start(); 
    _pool->addTask([this](){
       _rtspPusher.start(); 
    });
    
}
void RtspConnect::handleTeardown() {
    std::lock_guard<std::mutex> lock(_sessionMutex);

    auto it = _sessionMap.find(currentSessionId);
    if (it != _sessionMap.end()) {
        _sessionMap.erase(it);
    }
    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n\r\n";
    sendResponse(response);
    _rtspPusher.stop();
}
void RtspConnect::sendResponse(const std::string& response) {
    _connPtr->sendInLoop(response);
    std::cout << "send data to client:\n" << response << std::endl;
}

string RtspConnect::generateSessionId() {
    static std::atomic<int> counter{0};
    std::stringstream ss;
    ss << std::hex << time(nullptr) << "_" << counter++;
    return ss.str();
}