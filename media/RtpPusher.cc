#include "RtpPusher.h"
#include <functional>
#include <chrono>
#include <iostream>

RtpPusher::RtpPusher(std::shared_ptr<TcpConnection> conn,
                     std::shared_ptr<MediaReader> videoReader,
                     std::shared_ptr<MediaReader> audioReader)
    : _conn(conn), _videoReader(videoReader), _audioReader(audioReader),_running(true) {
    std::cout << "[RtpPusher] constructed, this=" << this << std::endl;
}

void RtpPusher::start() {
    using namespace std::chrono;
    auto startTime = steady_clock::now();
    auto nextVideoTime = startTime;
    auto nextAudioTime = startTime;

    _timerId = _conn->addPeriodicTimer(0, 1, [this, startTime, nextVideoTime, nextAudioTime]() mutable {
        auto now = steady_clock::now();
        if(!_running){
            _conn->removeTimer(_timerId);
            return;
        }
        // 先处理视频帧
        if (now >= nextVideoTime) {
            std::vector<uint8_t> nalu;
            auto status = _videoReader->readFrame(nalu);
            if (status == ReadStatus::Ok && _running) {
                uint8_t nalu_type = nalu[0] & 0x1F;
                if (nalu_type == 7) {
                    _sps = nalu;
                } else if (nalu_type == 8) {
                    _pps = nalu;
                } else if (nalu_type == 5) {
                    if (!_sps.empty()) sendH264Frame(_sps);
                    if (!_pps.empty()) sendH264Frame(_pps);
                    sendH264Frame(nalu);
                    _timestampVideo += 3600;
                    nextVideoTime += milliseconds(40);
                } else {
                    sendH264Frame(nalu);
                    _timestampVideo += 3600;
                    nextVideoTime += milliseconds(40);
                }
            } else if (status == ReadStatus::Eof) {
                std::cout << "[RtpPusher] H264 Read completed." << std::endl;
                _running = false;
                return;
            } else {
                std::cout << "[RtpPusher] H264 read error." << std::endl;
                _running = false;
                return;
            }
        }
        // 再处理音频帧
        if (now >= nextAudioTime) {
            std::vector<uint8_t> aac;
            auto status = _audioReader->readFrame(aac);
            if (status == ReadStatus::Ok && _running) {
                sendAacFrame(aac);
                _timestampAudio += 1920;
                nextAudioTime += milliseconds(21);
            } else if (status == ReadStatus::Eof) {
                std::cout << "[RtpPusher] AAC Read completed." << std::endl;
                _running = false;
                return;
            } else {
                std::cout << "[RtpPusher] AAC read error." << std::endl;
                _running = false;
                return;
            }
        }
    });
}

void RtpPusher::stop(){
    _running = false;
    _conn->removeTimer(_timerId);
}


static std::vector<uint8_t> buildRtpHeader(uint16_t seq, uint32_t timestamp, uint32_t ssrc, uint8_t pt, bool marker) {
    std::vector<uint8_t> h(12);
    h[0] = 0x80;
    h[1] = pt;
    if (marker) h[1] |= 0x80;
    h[2] = seq >> 8;
    h[3] = seq & 0xFF;
    h[4] = (timestamp >> 24) & 0xFF;
    h[5] = (timestamp >> 16) & 0xFF;
    h[6] = (timestamp >> 8) & 0xFF;
    h[7] = (timestamp) & 0xFF;
    h[8] = (ssrc >> 24) & 0xFF;
    h[9] = (ssrc >> 16) & 0xFF;
    h[10] = (ssrc >> 8) & 0xFF;
    h[11] = (ssrc) & 0xFF;
    return h;
}

void RtpPusher::sendH264Frame(const std::vector<uint8_t>& nalu) {
    const size_t mtu = 1400;
    if (nalu.size() + 12 <= mtu) {
        auto header = buildRtpHeader(_seqVideo++, _timestampVideo, _ssrcVideo, 96, true);
        std::vector<uint8_t> packet = header;
        packet.insert(packet.end(), nalu.begin(), nalu.end());
        uint8_t prefix[] = { '$', 0, uint8_t(packet.size() >> 8), uint8_t(packet.size() & 0xFF) };
        // std::cout << "[RtpPusher] Send H264 RTP packet, size=" << packet.size() << ", seq=" << _seqVideo-1 << std::endl;
        _conn->sendInLoop(std::string((char*)prefix, 4) + std::string((char*)packet.data(), packet.size()));
    } else {
        uint8_t nal_header = nalu[0];
        size_t pos = 1;
        bool isStart = true;
        while (pos < nalu.size()) {
            size_t len = std::min(mtu - 14, nalu.size() - pos);
            bool isLast = (pos + len == nalu.size());
            uint8_t fu_ind = (nal_header & 0xE0) | 28;
            uint8_t fu_hdr = (isStart ? 0x80 : 0x00) | (isLast ? 0x40 : 0x00) | (nal_header & 0x1F);

            std::vector<uint8_t> payload = { fu_ind, fu_hdr };
            payload.insert(payload.end(), nalu.begin() + pos, nalu.begin() + pos + len);

            auto header = buildRtpHeader(_seqVideo++, _timestampVideo, _ssrcVideo, 96, isLast);
            std::vector<uint8_t> packet = header;
            packet.insert(packet.end(), payload.begin(), payload.end());

            uint8_t prefix[] = { '$', 0, uint8_t(packet.size() >> 8), uint8_t(packet.size() & 0xFF) };
            // std::cout << "[RtpPusher] Send H264 RTP FU-A packet, size=" << packet.size() << ", seq=" << _seqVideo-1 << (isLast ? " [LAST]" : "") << std::endl;
            _conn->sendInLoop(std::string((char*)prefix, 4) + std::string((char*)packet.data(), packet.size()));

            pos += len;
            isStart = false;
        }
    }
}

void RtpPusher::sendAacFrame(const std::vector<uint8_t>& aac) {
    if (aac.size() < 7) return;

    auto header = buildRtpHeader(_seqAudio++, _timestampAudio, _ssrcAudio, 97, true);
    uint16_t aacLen = aac.size();
    uint8_t au_header[] = { 0x00, 0x10 }; // AU-headers-length: 16 bits
    uint8_t au_data[] = {
        uint8_t(aacLen >> 5),
        uint8_t((aacLen & 0x1F) << 3)
    };

    std::vector<uint8_t> payload;
    payload.insert(payload.end(), au_header, au_header + 2);
    payload.insert(payload.end(), au_data, au_data + 2);
    payload.insert(payload.end(), aac.begin(), aac.end());

    std::vector<uint8_t> packet = header;
    packet.insert(packet.end(), payload.begin(), payload.end());

    uint8_t prefix[] = { '$', 2, uint8_t(packet.size() >> 8), uint8_t(packet.size() & 0xFF) };
    // std::cout << "[RtpPusher] Send AAC RTP packet, size=" << packet.size() << ", seq=" << _seqAudio-1 << std::endl;
    _conn->sendInLoop(std::string((char*)prefix, 4) + std::string((char*)packet.data(), packet.size()));
}
