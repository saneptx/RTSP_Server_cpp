#include "RtpPusher.h"
#include <functional>
#include <chrono>
#include <iostream>
#include <thread>

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
: _useUdp(false)
, _running(false)
{

}
RtpPusher::RtpPusher(std::shared_ptr<TcpConnection> conn,
                     std::shared_ptr<MediaReader> videoReader,
                     std::shared_ptr<MediaReader> audioReader)
:_conn(conn)
,_videoReader(videoReader)
, _audioReader(audioReader)
, _useUdp(false)
,_running(true)
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
,_useUdp(true)
,_running(true){
    std::cout << "[RtpPusher] constructed Udp, this=" << this << std::endl;
}

void RtpPusher::start() {
    using namespace std::chrono;
    auto startTime = steady_clock::now();
    auto nextVideoTime = startTime;
    auto nextAudioTime = startTime;
    if (_useUdp) {
        if (!_videoRtpConn || !_audioRtpConn) {
            std::cerr << "[RtpPusher] UDP connections not initialized" << std::endl;
            return;
        }
    } else {
        if (!_conn) {
            std::cerr << "[RtpPusher] _conn == nullptr" << std::endl;
            return;
        }
    }
    // 根据传输模式选择定时器
    if (_useUdp) {
        _timerId = _videoRtpConn->addPeriodicTimer(0, 1, [this, startTime, nextVideoTime, nextAudioTime]() mutable {
            auto now = steady_clock::now();
            if(!_running){
                _videoRtpConn->removeTimer(_timerId);
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
                    sendAacFrameUdp(aac);
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
    } else {
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
                                uint8_t prefix[] = { '$', 0, uint8_t(packet.size() >> 8), uint8_t(packet.size() & 0xFF) };
                                _conn->sendInLoop(std::string((char*)prefix, 4) + std::string((char*)packet.data(), packet.size()));
                            } else {
                                if (!_sps.empty()) sendH264Frame(_sps);
                                if (!_pps.empty()) sendH264Frame(_pps);
                                sendH264Frame(nalu);
                            }
                        } else {
                            if (!_sps.empty()) sendH264Frame(_sps);
                            if (!_pps.empty()) sendH264Frame(_pps);
                            sendH264Frame(nalu);
                        }
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
}

void RtpPusher::stop(){
    _running = false;
    if (_useUdp) {
        _videoRtpConn->removeTimer(_timerId);
    } else if (_conn) {
        _conn->removeTimer(_timerId);
    }
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
            /*
            具体来说，P frame（预测帧）的解码错误，通常意味着它所依赖的前一个参考帧（I帧或P帧）在传输过程中发生了丢包。
            当你看到解码器报告“左侧块不可用”（left block unavailable）时，这非常明确地指向了同一个视频帧的内部数据丢失。
            这通常发生在当一个大的视频帧（无论是I帧还是P-帧）因为尺寸超过MTU（最大传输单元）而必须被分割成多个RTP包（使用FU-A分片机制）来发送时。
            你的代码在发送这些分片包时，是在一个非常紧凑的循环里一次性将它们全部发出的。
            这种短时间内的流量突发（burst）很容易超出网络中路由器或交换机的处理能力，导致其中一部分数据包被丢弃。
            只要丢失一个分片，整个视频帧就无法被正确解码，从而引发你看到的各种错误。
            最直接的解决方案是在发送这些分片包之间引入一个非常微小的延迟，这被称为“发包步调控制”（Packet Pacing）。
            这个小延迟可以给网络设备足够的时间来处理数据包，从而避免因流量突发造成的丢包。
             */
            if (pos < nalu.size()) {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
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
            if (pos < nalu.size()) {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
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
