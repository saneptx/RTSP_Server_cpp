#include "RtspConnect.h"
#include <iostream>
#include <sstream>
#include <atomic>
#include <thread>
#include <regex>
#include "../reactor/Logger.h"
using std::cout;
using std::endl;

//初始化静态成员
std::unordered_map<std::string, RtspSession> RtspConnect::_sessionMap;
std::mutex RtspConnect::_sessionMutex;

// 用于端口分配的静态变量
static std::atomic<int> nextUdpPort{8889};
static std::mutex portMutex;

RtspConnect::RtspConnect(TcpConnectionPtr connPtr,EventLoopPtr loopPtr)
:_connPtr(connPtr)
,_loopPtr(loopPtr)
,method("")
,url("")
,version("")
,CSeq(0)
,transport("")
,currentSessionId("")
,_h264FileReaderPtr(std::make_shared<H264FileReader>("data/1.h264"))
,_aacFileReaderPtr(std::make_shared<AacFileReader>("data/1.aac"))
// ,_rtspPusher(_connPtr,_h264FileReaderPtr,_aacFileReaderPtr)
{
    LOG_INFO("RtspConnect constructed - fd: %d, this: %p", connPtr->getFd(), this);
}

RtspConnect::~RtspConnect(){
    LOG_INFO("RtspConnect destructed - fd: %d, this: %p", _connPtr->getFd(), this);
    releaseUdpPorts();
}

void RtspConnect::handleRtspConnect(){

    std::string rBuf = _connPtr->reciveRtspRequest();
    if (rBuf.empty()) {
        LOG_DEBUG("No complete RTSP request received from fd: %d", _connPtr->getFd());
        // 没有完整请求
        return;
    }
    LOG_INFO("Received RTSP request from fd %d: %zu bytes", _connPtr->getFd(), rBuf.size());
    LOG_DEBUG("Request content:\n%s", rBuf.c_str());

    // 1. 解析请求
    parseRequest(rBuf);

    // 2. 路由处理
    if (method == "OPTIONS") {
        LOG_DEBUG("Handling OPTIONS request, CSeq: %d", CSeq);
        handleOptions();
    } else if (method == "DESCRIBE") {
        LOG_DEBUG("Handling DESCRIBE request, CSeq: %d", CSeq);
        handleDescribe();
    } else if (method == "SETUP") {
        LOG_DEBUG("Handling SETUP request, CSeq: %d", CSeq);
        handleSetup();
    } else if (method == "PLAY") {
        LOG_DEBUG("Handling PLAY request, CSeq: %d", CSeq);
        handlePlay();
    } else if (method == "TEARDOWN") {
        LOG_DEBUG("Handling TEARDOWN request, CSeq: %d", CSeq);
        handleTeardown();
    } else {
        LOG_WARN("Unknown RTSP method: %s, CSeq: %d", method.c_str(), CSeq);
        sendResponse("RTSP/1.0 400 Bad Request\r\nCSeq: " + std::to_string(CSeq) + "\r\n\r\n");
    }
    
}

void RtspConnect::releaseSession() {
    std::lock_guard<std::mutex> lock(_sessionMutex);
    if (!currentSessionId.empty()) {
        auto it = _sessionMap.find(currentSessionId);
        if (it != _sessionMap.end()) {
            if (it->second.useUdp) {
                LOG_DEBUG("Releasing UDP ports for session: %s", currentSessionId.c_str());
                releaseUdpPorts();
            }
            _sessionMap.erase(it);
            LOG_INFO("Session %s released", currentSessionId.c_str());
        }
        currentSessionId.clear();
    }
    if(_rtspPusher) {
        LOG_DEBUG("Stopping RTP pusher");
        _rtspPusher->stop();
    }
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
            LOG_DEBUG("Parsed request line: %s %s %s", method.c_str(), url.c_str(), version.c_str());
        } else if (h.find("CSeq") != std::string::npos || h.find("CSEQ") != std::string::npos) {
            size_t pos = h.find(":");
            if (pos != std::string::npos)
                CSeq = std::stoi(h.substr(pos + 1));
        } else if (h.find("Transport") != std::string::npos) {
            transport = h;
            LOG_DEBUG("Transport header: %s", transport.c_str());
        } else if (h.find("Session:") != std::string::npos){
            size_t pos = h.find(":");
            if (pos != std::string::npos)
                currentSessionId = h.substr(pos + 1);
            currentSessionId.erase(0, currentSessionId.find_first_not_of(" \t")); // 去空格
            LOG_DEBUG("Session ID: %s", currentSessionId.c_str());
        }
        // 你可以继续处理其他header
    }
}

void RtspConnect::handleOptions() {
    LOG_DEBUG("Sending OPTIONS response, CSeq: %d", CSeq);
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
    LOG_DEBUG("Generating SDP for IP: %s", localIP.c_str());
    
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
    LOG_DEBUG("Sending DESCRIBE response, CSeq: %d, SDP size: %zu", CSeq, sdp.size());
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
        LOG_INFO("Created new session: %s for client: %s", currentSessionId.c_str(), session.clientIP.c_str());
    }

    RtspSession& session = _sessionMap[currentSessionId];
    session.lastActive = time(nullptr);

    // 解析Transport头
    bool useUdp = false;
    InetAddress clientVideoAddr, clientAudioAddr;
    
    if (transport.find("RTP/AVP") != std::string::npos &&
              transport.find("interleaved=") == std::string::npos) {
        // UDP传输
        useUdp = true;
        session.useUdp = true;
        int basePort = allocateUdpPorts();
        if (session.serverVideoPort == 0) {
            session.serverVideoPort = basePort;
        }else if(session.serverAudioPort == 0){
            session.serverAudioPort = basePort + 2;
        }
        LOG_DEBUG("UDP transport detected, video port: %d, audio port: %d", 
                 session.serverVideoPort, session.serverAudioPort);
        
        // 解析客户端端口
        std::regex clientPortRegex(R"(client_port=(\d+)-(\d+))");
        std::smatch match;
        if (std::regex_search(transport, match, clientPortRegex)) {
            int clientRtpPort = std::stoi(match[1]);
            int clientRtcpPort = std::stoi(match[2]);
            
            if (url.find("track0") != std::string::npos) { // 视频
                session.clientVideoRtpAddr = InetAddress(_connPtr->getPeerAddr().ip(), clientRtpPort);
                session.clientVideoRtcpAddr = InetAddress(_connPtr->getPeerAddr().ip(), clientRtcpPort);
                _videoRtpConn = std::make_shared<UdpConnection>(_connPtr->getLocalAddr().ip(),session.serverVideoPort,session.clientVideoRtpAddr,_loopPtr);//建立视频Rtp连接
                _videoRtcpConn = std::make_shared<UdpConnection>(_connPtr->getLocalAddr().ip(),session.serverVideoPort+1,session.clientVideoRtcpAddr,_loopPtr);//建立视频Rtcp连接
                LOG_DEBUG("Created video UDP connections - RTP: %d, RTCP: %d", session.serverVideoPort, session.serverVideoPort+1);
            } else if (url.find("track1") != std::string::npos) { // 音频
                session.clientAudioRtpAddr = InetAddress(_connPtr->getPeerAddr().ip(), clientRtpPort);
                session.clientAudioRtcpAddr = InetAddress(_connPtr->getPeerAddr().ip(), clientRtcpPort);
                _audioRtpConn = std::make_shared<UdpConnection>(_connPtr->getLocalAddr().ip(),session.serverAudioPort,session.clientAudioRtpAddr,_loopPtr);//建立音频Rtp连接
                _audioRtcpConn = std::make_shared<UdpConnection>(_connPtr->getLocalAddr().ip(),session.serverAudioPort+1,session.clientAudioRtcpAddr,_loopPtr);//建立音频Rtcp连接
                LOG_DEBUG("Created audio UDP connections - RTP: %d, RTCP: %d", session.serverAudioPort, session.serverAudioPort+1);
            }
        }
    } else {
        LOG_DEBUG("TCP transport detected");
    }
    
    std::string response;
    if (url.find("track0") != std::string::npos) { // 视频
        if (useUdp) {
            response = "RTSP/1.0 200 OK\r\n"
                       "CSeq: " + std::to_string(CSeq) + "\r\n"
                       "Transport: RTP/AVP;unicast;client_port=" + 
                       std::to_string(session.clientVideoRtpAddr.port()) + "-" + 
                       std::to_string(session.clientVideoRtcpAddr.port()) + 
                       ";server_port=" + std::to_string(session.serverVideoPort) + "-" + 
                       std::to_string(session.serverVideoPort + 1) + "\r\n"
                       "Session: " + currentSessionId + "\r\n\r\n";
        } else {
            response = "RTSP/1.0 200 OK\r\n"
                       "CSeq: " + std::to_string(CSeq) + "\r\n"
                       "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                       "Session: " + currentSessionId + "\r\n\r\n";
        }
    } else if (url.find("track1") != std::string::npos) { // 音频
        if (useUdp) {
            response = "RTSP/1.0 200 OK\r\n"
                       "CSeq: " + std::to_string(CSeq) + "\r\n"
                       "Transport: RTP/AVP;unicast;client_port=" + 
                       std::to_string(session.clientAudioRtpAddr.port()) + "-" + 
                       std::to_string(session.clientAudioRtcpAddr.port()) + 
                       ";server_port=" + std::to_string(session.serverAudioPort) + "-" + 
                       std::to_string(session.serverAudioPort + 1) + "\r\n"
                       "Session: " + currentSessionId + "\r\n\r\n";
        } else {
            response = "RTSP/1.0 200 OK\r\n"
                       "CSeq: " + std::to_string(CSeq) + "\r\n"
                       "Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n"
                       "Session: " + currentSessionId + "\r\n\r\n";
        }
    } else {
        response = "RTSP/1.0 454 Session Not Found\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n\r\n";
    }
    LOG_DEBUG("Sending SETUP response, CSeq: %d, session: %s", CSeq, currentSessionId.c_str());
    sendResponse(response);
}

void RtspConnect::handlePlay() {
    std::lock_guard<std::mutex> lock(_sessionMutex);
    auto it = _sessionMap.find(currentSessionId);
    if (it == _sessionMap.end()) {
        LOG_WARN("Session not found: %s", currentSessionId.c_str());
        sendResponse("RTSP/1.0 454 Session Not Found\r\nCSeq: " + std::to_string(CSeq) + "\r\n\r\n");
        return;
    }

    it->second.isPlaying = true;
    it->second.lastActive = time(nullptr);
    LOG_INFO("Starting playback for session: %s", currentSessionId.c_str());
    
    if(it->second.useUdp){
        LOG_DEBUG("Starting UDP RTP pusher");
        this->_rtspPusher = std::make_shared<RtpPusher>(_videoRtpConn,_audioRtpConn,_h264FileReaderPtr,_aacFileReaderPtr);
        _loopPtr->addEpollReadFd(_videoRtcpConn->getUdpFd());
        _loopPtr->addEpollReadFd(_audioRtcpConn->getUdpFd());
        _loopPtr->udpConns[_videoRtcpConn->getUdpFd()] = _videoRtcpConn;
        _loopPtr->udpConns[_audioRtcpConn->getUdpFd()] = _audioRtcpConn;
        auto rtcpCallback = [](const UdpConnectionPtr &udpConn, const char* streamType){
            char buffer[2048];
            int n = udpConn->recv(buffer);
            if (n <= 0) {
                LOG_DEBUG("[RTCP] No data received for %s stream", streamType);
            }
            const uint8_t* packet = (const uint8_t*)buffer;
            size_t len = n;
            size_t pos = 0;

            while (pos + 4 <= len) { // Minimum RTCP header size
                uint16_t length_words = (packet[pos + 2] << 8) | packet[pos + 3];
                size_t packet_len_bytes = (length_words + 1) * 4;

                if (pos + packet_len_bytes > len) {
                    // Malformed packet, stop processing
                    break;
                }

                uint8_t pt = packet[pos + 1];
                if (pt == 201 && pos + 8 <= len) { // Receiver Report
                    // SSRC of the media source being reported on
                    uint32_t ssrc_source = ntohl(*(uint32_t*)&packet[pos + 8]);
                    LOG_DEBUG("[RTCP] Received Receiver Report for %s stream (SSRC: %u)", streamType, ssrc_source);
                }
                
                pos += packet_len_bytes;
            }
        };
        _videoRtcpConn->setMessageCallback([rtcpCallback](const UdpConnectionPtr &conn){
            rtcpCallback(conn, "video");
        });

        _audioRtcpConn->setMessageCallback([rtcpCallback](const UdpConnectionPtr &conn){
            rtcpCallback(conn, "audio");
        });
    }else{
        LOG_DEBUG("Starting TCP RTP pusher");
        this->_rtspPusher = std::make_shared<RtpPusher>(_connPtr,_h264FileReaderPtr,_aacFileReaderPtr);
    }
    
    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n"
                           "Session: " + currentSessionId + "\r\n\r\n";
    LOG_DEBUG("Sending PLAY response, CSeq: %d", CSeq);
    sendResponse(response);
    if(_rtspPusher) {
        LOG_INFO("Starting RTP pusher");
        _rtspPusher->start(); 
    }
}

void RtspConnect::handleTeardown() {
    std::lock_guard<std::mutex> lock(_sessionMutex);

    auto it = _sessionMap.find(currentSessionId);
    if (it != _sessionMap.end()) {
        LOG_INFO("Teardown session: %s", currentSessionId.c_str());
        _sessionMap.erase(it);
    }
    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n\r\n";
    LOG_DEBUG("Sending TEARDOWN response, CSeq: %d", CSeq);
    sendResponse(response);
    if(_rtspPusher) {
        LOG_DEBUG("Stopping RTP pusher");
        _rtspPusher->stop();
    }
}

void RtspConnect::sendResponse(const std::string& response) {
    LOG_DEBUG("Sending RTSP response to fd %d: %zu bytes", _connPtr->getFd(), response.size());
    LOG_DEBUG("Response content:\n%s", response.c_str());
    _connPtr->sendInLoop(response);
}

string RtspConnect::generateSessionId() {
    static std::atomic<int> counter{0};
    std::stringstream ss;
    ss << std::hex << time(nullptr) << "_" << counter++;
    string sessionId = ss.str();
    LOG_DEBUG("Generated session ID: %s", sessionId.c_str());
    return sessionId;
}

int RtspConnect::allocateUdpPorts() {
    std::lock_guard<std::mutex> lock(portMutex);
    int basePort = nextUdpPort.fetch_add(2); // 分配6个端口：视频RTP/RTCP, 音频RTP/RTCP, 控制RTP/RTCP
    LOG_DEBUG("Allocated UDP ports starting from: %d", basePort);
    return basePort;
}

void RtspConnect::releaseUdpPorts() {
    if(_videoRtcpConn){
        LOG_DEBUG("Removing video RTCP connection from epoll");
        _loopPtr->delEpollReadFd(_videoRtcpConn->getUdpFd());
        _loopPtr->udpConns.erase(_videoRtcpConn->getUdpFd());
    }
    if(_audioRtcpConn){
        LOG_DEBUG("Removing audio RTCP connection from epoll");
        _loopPtr->delEpollReadFd(_audioRtcpConn->getUdpFd());
        _loopPtr->udpConns.erase(_audioRtcpConn->getUdpFd());
    }
    _videoRtpConn.reset();
    _videoRtcpConn.reset();
    _audioRtpConn.reset();
    _audioRtcpConn.reset();
    LOG_DEBUG("UDP connections released");
}