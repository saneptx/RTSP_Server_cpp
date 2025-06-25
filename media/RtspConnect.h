#ifndef __RTSPCONNECT_H__
#define __RTSPCONNECT_H__
#include "../reactor/TcpConnection.h"
#include <string>
using std::string;

class RtspConnect{
public:
    RtspConnect(TcpConnectionPtr connPtr);
    ~RtspConnect();
    void handleRtspConnect();

private:
    void parseRequest(const std::string& rBuf);
    void handleOptions();
    void handleDescribe();
    void handleSetup();
    void handlePlay();
    void handleTeardown();
    void sendResponse(const std::string& response);
    TcpConnectionPtr _connPtr;
    string method,url,version;
    int CSeq = 0;
    string transport;
    string session = "1185d20035702ca";
};


#endif