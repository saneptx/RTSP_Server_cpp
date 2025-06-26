#include "H264FileReader.h"
#include <iostream>

H264FileReader::H264FileReader(const std::string& filepath) {
    _file.open(filepath, std::ios::binary);
}

H264FileReader::~H264FileReader() {
    if (_file.is_open()) {
        _file.close();
    }
}

bool H264FileReader::isStartCode(const uint8_t* p, size_t len) {
    return (len >= 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) ||
           (len >= 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01);
}

ReadStatus H264FileReader::readFrame(std::vector<uint8_t>& outFrame) {
    if (!_file.is_open()){
        std::cout<<"H.264 file open failed!"<<std::endl;
        //文件打开失败返回-1
        return ReadStatus::FileError;
    }else if(_file.eof()){
        std::cout<<"H.264 file read eof!"<<std::endl;
        //文件读取完毕返回0
        return ReadStatus::Eof;
    }

    std::vector<uint8_t> buffer;
    uint8_t byte;
    size_t startCodeLen = 0;

    // 1. 跳过文件开头的起始码
    while (_file.read((char*)&byte, 1)) {
        buffer.push_back(byte);
        if (isStartCode(buffer.data(), buffer.size())) {
            startCodeLen = (buffer.size() >= 4 && buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0x00 && buffer[3] == 0x01) ? 4 : 3;
            buffer.clear();
            break;
        }
    }

    // 2. 读取NALU内容，直到下一个起始码
    while (_file.read((char*)&byte, 1)) {
        buffer.push_back(byte);
        size_t sz = buffer.size();
        if ((sz >= 4 && isStartCode(&buffer[sz - 4], 4)) ||
            (sz >= 3 && isStartCode(&buffer[sz - 3], 3))) {
            // 找到下一个起始码，回退到起始码前
            size_t codeLen = (sz >= 4 && isStartCode(&buffer[sz - 4], 4)) ? 4 : 3;
            buffer.resize(sz - codeLen);
            _file.seekg(-static_cast<int>(codeLen), std::ios::cur);
            break;
        }
    }

    if (buffer.empty()){ 
        std::cout<<"H.264 file read failed!"<<std::endl;
        return ReadStatus::NoData;
    }
    outFrame = std::move(buffer);
    return ReadStatus::Ok;
}