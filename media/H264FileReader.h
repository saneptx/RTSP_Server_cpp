#ifndef __H264FILEREADER_H__
#define __H264FILEREADER_H__

#include "MediaReader.h"
#include <fstream>
#include <string>

class H264FileReader : public MediaReader {
public:
    explicit H264FileReader(const std::string& filepath);
    ~H264FileReader();

    ReadStatus readFrame(std::vector<uint8_t>& outFrame) override;

private:
    std::ifstream _file;
    bool isStartCode(const uint8_t* p,size_t len);

};


#endif