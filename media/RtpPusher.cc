#include "RtpPusher.h"
#include <thread>
#include <chrono>
#include <iostream>

RtpPusher::RtpPusher(std::shared_ptr<TcpConnection> conn,
                     std::shared_ptr<MediaReader> videoReader,
                     std::shared_ptr<MediaReader> audioReader)
    : _conn(conn), _videoReader(videoReader), _audioReader(audioReader) {
    std::cout << "[RtpPusher] constructed, this=" << this << std::endl;
}

void RtpPusher::start() {
    _running = true;
    std::cout << "[RtpPusher] start streaming..." << std::endl;
    _thread = std::thread(&RtpPusher::sendLoop, this);
}

void RtpPusher::stop() {
    _running = false;
    std::cout << "[RtpPusher] stop streaming..." << std::endl;
    _thread.join();
}

void RtpPusher::sendLoop() {
    std::cout << "[RtpPusher] sendLoop started, this=" << this << std::endl;

    auto lastVideoTime = std::chrono::steady_clock::now();
    auto lastAudioTime = lastVideoTime;

    const int videoInterval = 1000 / 25;     // 25fps → 40ms
    const int audioInterval = 1000 * 1024 / 48000; // AAC帧间隔 ≈ 21.3ms

    while (!_conn->isClosed()&&_running) {
        auto now = std::chrono::steady_clock::now();

        if (_videoReader && std::chrono::duration_cast<std::chrono::milliseconds>(now - lastVideoTime).count() >= videoInterval) {
            std::vector<uint8_t> nalu;
            if (_videoReader->readFrame(nalu)==ReadStatus::Ok) {
                // std::cout << "[RtpPusher] Read H264 frame, size=" << nalu.size() << std::endl;
                sendH264Frame(nalu);
                _timestampVideo += 3600;
            } else if(_videoReader->readFrame(nalu)==ReadStatus::Eof){
                std::cout << "[RtpPusher] Read complete." << std::endl;
                break;
            } else {
                std::cout << "[RtpPusher] Read error." << std::endl;
                break;
            }
            lastVideoTime = now;
        }

        if (_audioReader && std::chrono::duration_cast<std::chrono::milliseconds>(now - lastAudioTime).count() >= audioInterval) {
            std::vector<uint8_t> aac;
            if (_audioReader->readFrame(aac)==ReadStatus::Ok) {
                // std::cout << "[RtpPusher] Read AAC frame, size=" << aac.size() << std::endl;
                sendAacFrame(aac);
                _timestampAudio += 1024 * 90000 / 48000; // → 1920
            } else if(_audioReader->readFrame(aac)==ReadStatus::Eof){
                std::cout << "[RtpPusher] Read complete." << std::endl;
                break;
            } else{
                std::cout << "[RtpPusher] No more AAC frame or read error." << std::endl;
                break;
            }
            lastAudioTime = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 更细粒度休眠
    }

    std::cout << "[RtpPusher] sendLoop exited, this=" << this << std::endl;
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
        uint8_t fu_ind = (nal_header & 0xE0) | 28;
        uint8_t fu_hdr = 0x80 | (nal_header & 0x1F); // Start bit

        size_t pos = 1;
        while (pos < nalu.size()) {
            size_t len = std::min((size_t)mtu - 14, nalu.size() - pos);
            std::vector<uint8_t> payload = { fu_ind, fu_hdr };
            payload.insert(payload.end(), nalu.begin() + pos, nalu.begin() + pos + len);

            bool isLast = (pos + len == nalu.size());
            if (isLast) fu_hdr |= 0x40; // 设置 End bit

            auto header = buildRtpHeader(_seqVideo++, _timestampVideo, _ssrcVideo, 96, isLast);
            std::vector<uint8_t> packet = header;
            packet.insert(packet.end(), payload.begin(), payload.end());

            uint8_t prefix[] = { '$', 0, uint8_t(packet.size() >> 8), uint8_t(packet.size() & 0xFF) };
            // std::cout << "[RtpPusher] Send H264 RTP FU-A packet, size=" << packet.size() << ", seq=" << _seqVideo-1 << (isLast ? " [LAST]" : "") << std::endl;
            _conn->sendInLoop(std::string((char*)prefix, 4) + std::string((char*)packet.data(), packet.size()));

            pos += len;
            fu_hdr &= ~0x80; // 清除 Start bit
            if (isLast) fu_hdr &= ~0x40; // 清除 End bit
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
