#ifndef __MEDIAREADER_H__
#define __MEDIAREADER_H__

#include <vector>
#include <cstdint>


enum class ReadStatus {
    Ok,             // 正常读取到一帧数据
    Eof,            // 读取到文件末尾
    FileError,      // 文件打开失败或读取错误
    NoData          // 当前暂时没有数据（如缓冲区为空）
};


class MediaReader{

public:
    // 读取一帧（成功返回 true，失败/文件结束返回 false）
    virtual ReadStatus readFrame(std::vector<uint8_t>& outFrame) = 0;

    virtual ~MediaReader() = default;
};



#endif