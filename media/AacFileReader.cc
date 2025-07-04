#include "AacFileReader.h"
#include <iostream>
#include <string.h>
#include "../reactor/Logger.h"

AacFileReader::AacFileReader(const std::string& filepath) {
    _file.open(filepath, std::ios::binary);
}

AacFileReader::~AacFileReader() {
    if (_file.is_open()) {
        _file.close();
    }
}

ReadStatus AacFileReader::readFrame(std::vector<uint8_t>& outFrame) {
    outFrame.clear();
    if (!_file.is_open()) {
        LOG_ERROR("Aac file open failed!");
        return ReadStatus::FileError;
    }

    uint8_t header[7];
    if (!_file.read(reinterpret_cast<char*>(header), 7)) {
        if (_file.eof()) {
            LOG_INFO("Aac file read eof!");
            return ReadStatus::Eof;
        }
        return ReadStatus::FileError;
    }

    // 校验 ADTS 同步头（12bit）
    if (header[0] != 0xFF || (header[1] & 0xF0) != 0xF0) return ReadStatus::FileError;

    // aac_frame_length: 13 bits: bits 30-42
    uint16_t frameLength = ((header[3] & 0x03) << 11) |
                           (header[4] << 3) |
                           ((header[5] & 0xE0) >> 5);

    if (frameLength < 7) return ReadStatus::FileError; // 长度不对

    outFrame.resize(frameLength);
    ::memcpy(outFrame.data(), header, 7); // 包括 ADTS 头
    if (!_file.read(reinterpret_cast<char*>(outFrame.data() + 7), frameLength - 7)) {
        if (_file.eof()) {
            LOG_INFO("Aac file read eof!");
            outFrame.clear();
            return ReadStatus::Eof;
        }
        LOG_DEBUG("Aac file read failed!");
        outFrame.clear();
        return ReadStatus::NoData;
    }

    return ReadStatus::Ok;
}