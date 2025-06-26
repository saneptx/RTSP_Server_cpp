#ifndef __AACFILEREADER_H__
#define __AACFILEREADER_H__

#include "MediaReader.h"
#include <fstream>
#include <string>

class AacFileReader : public MediaReader {
public:
    explicit AacFileReader(const std::string& filepath);
    ~AacFileReader();

    ReadStatus readFrame(std::vector<uint8_t>& outFrame) override;

private:
    std::ifstream _file;
};

#endif