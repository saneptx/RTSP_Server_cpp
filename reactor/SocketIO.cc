#include "SocketIO.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <string>
using std::string;

SocketIO::SocketIO(int fd)
:_fd(fd){

}
SocketIO::~SocketIO(){
    close(_fd);
}
int SocketIO::readn(char *buf,int len){
    int left = len;
    char *pstr = buf;
    int ret = 0;
    
    while(left > 0){
        ret = read(_fd,pstr,left);
        if(-1 == ret && errno == EINTR){
            continue;
        }else if(-1 == ret){
            perror("read error -1");
            return -1;
        }else if(0 == ret){
            break;
        }else{
            pstr += ret;
            left -= ret;
        }
    }
    return len - left;
}
int SocketIO::readLine(char *buf,int len){
    int left = len -1;
    char *pstr = buf;
    int ret = 0, total = 0;
    while(left > 0){
        ret = recv(_fd,pstr,left,MSG_PEEK);//MSG_PEEK不会将缓冲区中的数据进行清空，只会进行拷贝操作
        if(-1 == ret && errno == EINTR){
            continue;
        }else if(-1 == ret){
            perror("readLine error -1");
            return -1;
        }else if(0 == ret){
            break;
        }else{
            for(int idx = 0;idx < ret;++idx){
                if(pstr[idx] == '\n'){
                    int sz = idx + 1;
                    readn(pstr,sz);
                    pstr += sz;
                    *pstr == '\0';//c风格以'\0'结尾
                    return total + sz;
                }
            }
            readn(pstr,ret);
            total += ret;
            pstr += ret;
            left -= ret;
        }
    }
    *pstr = '\0';
    return total - left;
}
int SocketIO::readCRLFCRLF(char *buf, int len){
    int left = len - 1; // 留一个位置给 '\0'
    char *pstr = buf;
    int total = 0;

    std::string tempBuf;

    while (left > 0) {
        char peekBuf[1024] = {0};
        int peekLen = (left < sizeof(peekBuf)) ? left : sizeof(peekBuf);

        int ret = recv(_fd, peekBuf, peekLen, MSG_PEEK);
        if (ret == -1 && errno == EINTR) {
            continue;
        } else if (ret == -1) {
            perror("readHeaderCRLFCRLF recv error");
            return -1;
        } else if (ret == 0) {
            // 对端关闭连接
            break;
        }

        // 将 peek 的内容追加到 tempBuf
        tempBuf.append(peekBuf, ret);

        // 查找 \r\n\r\n
        size_t pos = tempBuf.find("\r\n\r\n");
        if (pos != std::string::npos) {
            int headerLen = pos + 4; // 包含 \r\n\r\n

            if (headerLen > left) {
                std::cerr << "Header too large for buffer!" << std::endl;
                return -1;
            }

            // 读取实际内容
            int nread = readn(pstr, headerLen);
            if (nread != headerLen) {
                return -1;
            }

            pstr[headerLen] = '\0';
            return headerLen;
        }

        // 如果没找到，读取当前 peek 长度，避免死循环
        int toRead = ret;
        if (toRead > left) toRead = left;

        int nread = readn(pstr, toRead);
        if (nread != toRead) {
            return -1;
        }

        total += toRead;
        pstr += toRead;
        left -= toRead;
    }

    // 到这里说明没有遇到 \r\n\r\n，强制结尾
    *pstr = '\0';
    return total;
}
int SocketIO::writen(const char *buf,int len){
    int left = len;
    const char *pstr = buf;
    int ret = 0;
    while(left > 0){
        ret = write(_fd,pstr,left);
        if(-1 == ret && errno == EINTR){
            continue;
        }else if(-1 == ret){
            perror("writen error -1");
            return -1;
        }else if(0 == ret){
            break;
        }else{
            pstr += ret;
            left -= ret;
        }
    }
    return len - left;
}