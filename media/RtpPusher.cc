#include "RtpPusher.h"
#include <functional>
#include <chrono>
#include <iostream>
#include "../reactor/Logger.h"

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

RtpPusher::RtpPusher()
: _running(false)
, _useUdp(false)
{

}
RtpPusher::RtpPusher(std::shared_ptr<TcpConnection> conn,
                     std::shared_ptr<MediaReader> videoReader,
                     std::shared_ptr<MediaReader> audioReader)
:_conn(conn)
,_videoReader(videoReader)
, _audioReader(audioReader)
, _running(true)
, _useUdp(false)
{
    std::cout << "[RtpPusher] constructed Tcp, this=" << this << std::endl;
}
RtpPusher::RtpPusher(std::shared_ptr<UdpConnection> videoRtpConn,
            std::shared_ptr<UdpConnection> audioRtpConn,
            std::shared_ptr<MediaReader> videoReader,
            std::shared_ptr<MediaReader> audioReader)
:_videoRtpConn(videoRtpConn)
,_audioRtpConn(audioRtpConn)
,_videoReader(videoReader)
, _audioReader(audioReader)
, _running(true)
,_useUdp(true){
    std::cout << "[RtpPusher] constructed Udp, this=" << this << std::endl;
}

void RtpPusher::start() {
    using namespace std::chrono;
    auto startTime = steady_clock::now();
    auto nextVideoTime = startTime;
    auto nextAudioTime = startTime;
    if (_useUdp) {
        if (!_videoRtpConn || !_audioRtpConn) {
            LOG_ERROR("UDP connections not initialized");
            return;
        }
    } else {
        if (!_conn) {
            LOG_ERROR("_conn == nullptr");
            return;
        }
    }
    if (_useUdp) {
        _timerId = _videoRtpConn->addPeriodicTimer(0, 1, [this, startTime, nextVideoTime, nextAudioTime]() mutable {
            auto now = steady_clock::now();
            if(!_running){
                if (this->_timerId != 0) {
                    _videoRtpConn->removeTimer(_timerId);
                    this->_timerId = 0;
                }
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
                        /*
                        你当前的代码在发送I帧时，会先发送SPS包，然后发送PPS包，最后再发送I帧数据包。
                        这三个包是通过UDP独立发送的。由于UDP是不可靠的协议，网络中的任何抖动都可能导致其中任意一个包（例如SPS或PPS包）丢失。
                        如果客户端的解码器收到了I帧，但没有收到解码它所必需的SPS或PPS，就会报告 Missing reference picture 或类似的错误，
                        并尝试“隐藏错误”（concealing errors），这通常表现为视频画面出现花屏、卡顿或灰色块。
                        为了解决这个问题，我们可以采用RTP的一个高级特性，叫做聚合包（Aggregation Packet），
                        具体来说是 STAP-A (Single-Time Aggregation Packet)。
                        STAP-A 允许我们将多个小的NALU（如SPS、PPS和I帧）捆绑成一个单一的RTP包来发送。
                        这样做的好处是，它们要么一起成功到达，要么一起丢失。
                        这就从根本上避免了解码器收到一个不完整的关键帧数据，从而大大提高了在有损网络下的视频播放稳定性。
                         */
                        const size_t mtu = 1400;
                        if (!_sps.empty() && !_pps.empty()) {
                            size_t sps_size = _sps.size();
                            size_t pps_size = _pps.size();
                            size_t nalu_size = nalu.size();
                            size_t total_nalu_size = 1 + (2 + sps_size) + (2 + pps_size) + (2 + nalu_size);
                            
                            if (total_nalu_size + 12 <= mtu) { // STAP-A
                                uint8_t stap_header = (nalu[0] & 0x60) | 24;
                                std::vector<uint8_t> payload;
                                payload.push_back(stap_header);
                                payload.push_back(sps_size >> 8); payload.push_back(sps_size & 0xFF); payload.insert(payload.end(), _sps.begin(), _sps.end());
                                payload.push_back(pps_size >> 8); payload.push_back(pps_size & 0xFF); payload.insert(payload.end(), _pps.begin(), _pps.end());
                                payload.push_back(nalu_size >> 8); payload.push_back(nalu_size & 0xFF); payload.insert(payload.end(), nalu.begin(), nalu.end());
                                
                                auto rtp_header = buildRtpHeader(_seqVideo++, _timestampVideo, _ssrcVideo, 96, true);
                                std::vector<uint8_t> packet = rtp_header;
                                packet.insert(packet.end(), payload.begin(), payload.end());
                                _videoRtpConn->sendInLoop(std::string((char*)packet.data(), packet.size()));
                            } else {
                                sendH264FrameUdp(_sps);
                                sendH264FrameUdp(_pps);
                                sendH264FrameUdp(nalu);
                            }
                        } else {
                            if (!_sps.empty()) sendH264FrameUdp(_sps);
                            if (!_pps.empty()) sendH264FrameUdp(_pps);
                            sendH264FrameUdp(nalu);
                        }
                        _timestampVideo += 3600;
                        nextVideoTime += milliseconds(40);
                    } else {
                        sendH264FrameUdp(nalu);
                        _timestampVideo += 3600;
                        nextVideoTime += milliseconds(40);
                    }
                } else if (status == ReadStatus::Eof) {
                    LOG_INFO("H264 Read completed.");
                    _running = false;
                    return;
                } else {
                    LOG_ERROR("H264 read error.");
                    _running = false;
                    return;
                }
            }
            // 再处理音频帧
            if (now >= nextAudioTime) {
                std::vector<uint8_t> aac;
                auto status = _audioReader->readFrame(aac);
                if (status == ReadStatus::Ok && _running) {
                    sendAacFrameUdp(aac);
                    _timestampAudio += 1920;
                    nextAudioTime += milliseconds(21);
                } else if (status == ReadStatus::Eof) {
                    LOG_INFO("AAC Read completed.");
                    _running = false;
                    return;
                } else {
                    LOG_ERROR("AAC read error.");
                    _running = false;
                    return;
                }
            }
        });
    } else {
        this->_timerId = _conn->addPeriodicTimer(0, 1, [this, startTime, nextVideoTime, nextAudioTime]() mutable {
            // if (this->_timerId == 0) return; // 已经移除，不再做任何事
            auto now = steady_clock::now();
            if(!_running){
                if (this->_timerId != 0) {
                    _conn->removeTimer(_timerId);
                    this->_timerId = 0;
                }
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
                    LOG_INFO("H264 Read completed.");
                    _running = false;
                    return;
                } else {
                    LOG_ERROR("H264 read error.");
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
                    LOG_INFO("AAC Read completed.");
                    _running = false;
                    return;
                } else {
                    LOG_ERROR("AAC read error.");
                    _running = false;
                    return;
                }
            }
        });
    }
}

void RtpPusher::stop(){
    _running = false;
    if (_timerId != 0) {
        if (_useUdp) {
            _videoRtpConn->removeTimer(_timerId);
        } else if (_conn) {
            _conn->removeTimer(_timerId);
        }
        _timerId = 0;
    }
}

void RtpPusher::sendH264Frame(const std::vector<uint8_t>& nalu) {
    const size_t mtu = 1400;
    if (nalu.size() + 12 <= mtu) {
        auto header = buildRtpHeader(_seqVideo++, _timestampVideo, _ssrcVideo, 96, true);
        std::vector<uint8_t> packet = header;
        packet.insert(packet.end(), nalu.begin(), nalu.end());
        uint8_t prefix[] = { '$', 0, uint8_t(packet.size() >> 8), uint8_t(packet.size() & 0xFF) };
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
    _conn->sendInLoop(std::string((char*)prefix, 4) + std::string((char*)packet.data(), packet.size()));
}

void RtpPusher::sendH264FrameUdp(const std::vector<uint8_t>& nalu) {
    const size_t mtu = 1400;
    if (nalu.size() + 12 <= mtu) {
        auto header = buildRtpHeader(_seqVideo++, _timestampVideo, _ssrcVideo, 96, true);
        std::vector<uint8_t> packet = header;
        packet.insert(packet.end(), nalu.begin(), nalu.end());
        
        std::string packetStr((char*)packet.data(), packet.size());
        _videoRtpConn->sendInLoop(packetStr);
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

            std::string packetStr((char*)packet.data(), packet.size());
            _videoRtpConn->sendInLoop(packetStr);

            pos += len;
            isStart = false;
        }
    }
}

void RtpPusher::sendAacFrameUdp(const std::vector<uint8_t>& aac) {
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

    std::string packetStr((char*)packet.data(), packet.size());
    _audioRtpConn->sendInLoop(packetStr);
}
