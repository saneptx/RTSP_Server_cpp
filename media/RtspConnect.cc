#include "RtspConnect.h"
#include <iostream>
#include <sstream>
using std::cout;
using std::endl;



RtspConnect::RtspConnect(TcpConnectionPtr connPtr)
:_connPtr(connPtr){

}
RtspConnect::~RtspConnect(){

}
void RtspConnect::handleRtspConnect(){
    std::string rBuf = _connPtr->reciveRtspRequest();
    if (rBuf.empty()) {
        std::cout << "failed to recv data from client" << std::endl;
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

    // 清空状态
    // method.clear(); url.clear(); version.clear(); CSeq = 0; transport.clear();

}
void RtspConnect::parseRequest(const std::string& rBuf) {
    std::istringstream iss(rBuf);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("RTSP/") != std::string::npos) {
            std::istringstream lss(line);
            lss >> method >> url >> version;
        } else if (line.find("CSeq") != std::string::npos || line.find("CSEQ") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos)
                CSeq = std::stoi(line.substr(pos + 1));
        } else if (line.find("Transport") != std::string::npos) {
            transport = line;
        }
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
                      "a=control:track1\r\n";

    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n"
                           "Content-Base: rtsp://" + localIP + "/\r\n"
                           "Content-Type: application/sdp\r\n"
                           "Content-Length: " + std::to_string(sdp.size()) + "\r\n\r\n" + sdp;
    sendResponse(response);
}
void RtspConnect::handleSetup() {
    std::string response;
    if (url.find("track0") != std::string::npos) { // 视频
        response = "RTSP/1.0 200 OK\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n"
                   "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                   "Session: " + session + "\r\n\r\n";
    } else if (url.find("track1") != std::string::npos) { // 音频
        response = "RTSP/1.0 200 OK\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n"
                   "Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n"
                   "Session: " + session + "\r\n\r\n";
    } else {
        response = "RTSP/1.0 454 Session Not Found\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n\r\n";
    }
    sendResponse(response);
}
void RtspConnect::handlePlay() {
    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n"
                           "Session: " + session + "\r\n\r\n";
    sendResponse(response);
}
void RtspConnect::handleTeardown() {
    std::string response = "RTSP/1.0 200 OK\r\n"
                           "CSeq: " + std::to_string(CSeq) + "\r\n\r\n";
    sendResponse(response);
}
void RtspConnect::sendResponse(const std::string& response) {
    _connPtr->sendInLoop(response);
    std::cout << "send data to client:\n" << response << std::endl;
}

#ifdef __OLD_HANDLE__
void RtspConnect::handleRtspConnect(){
    std::string method, url, version;
    int CSeq=0;

    while(true){
        std::string rBuf = _connPtr->recive();
        if(rBuf.empty()){
            cout << "failed to recv data from client" << endl;
            break;
        }
        cout << "recv data from client:\n" << rBuf << endl;

        std::istringstream iss(rBuf);
        string line;
        while (std::getline(iss, line)) {
            if (line.find("OPTIONS") != std::string::npos ||
                line.find("DESCRIBE") != std::string::npos ||
                line.find("SETUP") != std::string::npos ||
                line.find("PLAY") != std::string::npos) {
                std::istringstream lss(line);
                lss >> method >> url >> version;
            } else if (line.find("CSeq") != std::string::npos) {
                size_t pos = line.find(":");
                if (pos != std::string::npos) {
                    CSeq = std::stoi(line.substr(pos + 1));
                }
            } else if (line.find("Transport") != std::string::npos) {
                // 这里可以用正则或string方法解析Transport内容

            }
        }
        string sBuf;
        if(method == "OPTIONS"){
            sBuf = "RTSP/1.0 200 OK\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n"
                   "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"
                   "\r\n";
        }else if(method == "DESCRIBE"){
            std::string sdp, localIP;
            // 解析localIP
            size_t start = url.find("rtsp://");
            if (start != std::string::npos) {
                start += 7;
                size_t end = url.find(":", start);
                if (end != std::string::npos) {
                    localIP = url.substr(start, end - start);
                }
            }
            sdp = "v=0\r\n"
                  "o=- 9" + std::to_string(time(NULL)) + " 1 IN IP4 " + localIP + "\r\n"
                  "s=Unnamed\r\n"
                  "t=0 0\r\n"
                  "a=control:*\r\n"
                  "m=video 0 RTP/AVP 96\r\n"
                  "a=rtpmap:96 H264/90000\r\n"
                  "a=control:track0\r\n"
                  "m=audio 1 RTP/AVP/TCP 97\r\n"
                  "a=rtpmap:97 mpeg4-generic/44100/2\r\n"
                  "a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210;\r\n"
                  "a=control:track1\r\n";
            sBuf = "RTSP/1.0 200 OK\r\n"
                   "CSeq: " + std::to_string(CSeq) + "\r\n"
                   "Content-Base: rtsp://" + localIP + "/\r\n"
                   "Content-Type: application/sdp\r\n"
                   "Content-Length: " + std::to_string(sdp.size()) + "\r\n"
                   "\r\n" + sdp;
        }else if(method == "SETUP"){
            if(CSeq == 3){
                sBuf = "RTSP/1.0 200 OK\r\n"
                        "CSeq: " + std::to_string(CSeq) + "\r\n"
                        "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                        "Session: 1185d20035702ca\r\n"
                        "\r\n";
            }else if(CSeq == 4){
                sBuf = "RTSP/1.0 200 OK\r\n"
                       "CSeq: " + std::to_string(CSeq) + "\r\n"
                       "Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n"
                        "Session: 1185d20035702ca\r\n"
                        "\r\n";
            }
        }else if(method == "PLAY"){
            sBuf = "RTSP/1.0 200 OK\r\n"
                    "CSeq: " + std::to_string(CSeq) + "\r\n"
                    "Session: 1185d20035702ca\r\n"
                    "\r\n";
        }else if (method == "TEARDOWN"){
            sBuf = "RTSP/1.0 200 OK\r\n"
                    "CSeq: " + std::to_string(CSeq) + "\r\n"
                    "\r\n";
        }else{//无法解析的请求
            sBuf = "RTSP/1.0 400 Bad Request\r\n"
                    "CSeq: "+ std::to_string(CSeq) + "\r\n"
                    "\r\n";
        }
        _connPtr->sendInLoop(sBuf);
        cout<<"send data to client:"<<endl<<sBuf<<endl;
    }
}
#endif