#include "H264FileReader.h"
#include <iostream>
#include "../reactor/Logger.h"

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
        LOG_ERROR("H.264 file open failed!");
        return ReadStatus::FileError;
    }

    std::vector<uint8_t> buffer;
    uint8_t byte;

    // 跳过文件开头的起始码
    while (_file.read((char*)&byte, 1)) {
        buffer.push_back(byte);
        if (isStartCode(buffer.data(), buffer.size())) {
            buffer.clear();
            break;
        }
    }

    // 读取NALU内容，直到下一个起始码
    while (_file.read((char*)&byte, 1)) {
        buffer.push_back(byte);
        size_t sz = buffer.size();
        if ((sz >= 4 && isStartCode(&buffer[sz - 4], 4)) ||
            (sz >= 3 && isStartCode(&buffer[sz - 3], 3))) {
            size_t codeLen = (sz >= 4 && isStartCode(&buffer[sz - 4], 4)) ? 4 : 3;
            buffer.resize(sz - codeLen);
            _file.seekg(-static_cast<int>(codeLen), std::ios::cur);
            break;
        }
    }

    if (buffer.empty()) {
        if (_file.eof()) {
            LOG_INFO("H.264 file read eof!");
            return ReadStatus::Eof;
        }
        LOG_DEBUG("H.264 file read failed!");
        return ReadStatus::NoData;
    }
    outFrame = std::move(buffer);
    return ReadStatus::Ok;
}