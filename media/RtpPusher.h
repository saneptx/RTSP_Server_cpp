#ifndef __RTPPUSHER_H__
#define __RTPPUSHER_H__

#include <memory>
#include <atomic>
#include <vector>
#include "MediaReader.h"
#include "../reactor/TcpConnection.h"
#include "../reactor/UdpConnection.h"

enum class ReadStatus;
class RtpPusher {
public:
    RtpPusher();
    RtpPusher(std::shared_ptr<TcpConnection> conn,
              std::shared_ptr<MediaReader> videoReader,
              std::shared_ptr<MediaReader> audioReader);

    RtpPusher(std::shared_ptr<UdpConnection> videoRtpConn,
              std::shared_ptr<UdpConnection> audioRtpConn,
              std::shared_ptr<MediaReader> videoReader,
              std::shared_ptr<MediaReader> audioReader);

    void start();
    void stop();
    
    void setTransportMode(bool useUdp, const InetAddress& videoAddr = InetAddress(), const InetAddress& audioAddr = InetAddress());
    
private:
    void sendH264Frame(const std::vector<uint8_t>& nalu);
    void sendAacFrame(const std::vector<uint8_t>& aac);
    void sendLoop();
    
    void sendH264FrameUdp(const std::vector<uint8_t>& nalu);
    void sendAacFrameUdp(const std::vector<uint8_t>& aac);
    
    std::shared_ptr<TcpConnection> _conn;
    std::shared_ptr<UdpConnection> _videoRtpConn;
    std::shared_ptr<UdpConnection> _audioRtpConn;
    std::shared_ptr<MediaReader> _videoReader;
    std::shared_ptr<MediaReader> _audioReader;

    std::atomic_bool _running;
    uint16_t _seqVideo = 0;
    uint16_t _seqAudio = 0;
    uint32_t _timestampVideo = 0;
    uint32_t _timestampAudio = 0;
    const uint32_t _ssrcVideo = 0x12345678;
    const uint32_t _ssrcAudio = 0x87654321;
    TimerId _timerId;
    std::vector<uint8_t> _sps, _pps;
    
    bool _useUdp = false;
};

#endif