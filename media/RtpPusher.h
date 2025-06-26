#ifndef __RTPPUSHER_H__
#define __RTPPUSHER_H__

#include <memory>
#include <atomic>
#include <vector>
#include "MediaReader.h"
#include "../reactor/TcpConnection.h" // 你的已有类

enum class ReadStatus;
class RtpPusher {
public:
    RtpPusher(std::shared_ptr<TcpConnection> conn,
              std::shared_ptr<MediaReader> videoReader,
              std::shared_ptr<MediaReader> audioReader);

    void sendLoop();
private:
    void sendH264Frame(const std::vector<uint8_t>& nalu);
    void sendAacFrame(const std::vector<uint8_t>& aac);

    std::shared_ptr<TcpConnection> _conn;
    std::shared_ptr<MediaReader> _videoReader;
    std::shared_ptr<MediaReader> _audioReader;

    uint16_t _seqVideo = 0;
    uint16_t _seqAudio = 0;
    uint32_t _timestampVideo = 0;
    uint32_t _timestampAudio = 0;
    const uint32_t _ssrcVideo = 0x12345678;
    const uint32_t _ssrcAudio = 0x87654321;
};

#endif