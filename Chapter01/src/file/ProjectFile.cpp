#include <string>
#include <regex>
#include <iomanip>

#include "./Util.hpp"
#include "./ProjectFile.hpp"
#include "../App.hpp"
#include "../misc/StringAlgo.hpp"
#include "../misc/MathUtil.hpp"
#include "../device/AudioDeviceManager.hpp"

NS_HWM_BEGIN

namespace {
    std::string const kProjectFileFormatID_v1 = "project_file_format_v1";
}

ProjectFile::FailedToParse::FailedToParse(std::string const &error_msg)
:   std::ios_base::failure("Failed to parse: " + error_msg)
{}

void ProjectFile::ScanAudioDeviceStatus()
{
    auto adm = AudioDeviceManager::GetInstance();
    auto dev = adm->GetDevice();
    
    if(!dev) { return; }
    
    sample_rate_ = dev->GetSampleRate();
    block_size_ = dev->GetBlockSize();
}

std::ostream & operator<<(std::ostream &os, ProjectFile const &self)
{
#define WRITE_MEMBER(name) \
<< (#name + std::string(" = ")) << std::quoted(to_s(self. name ## _)) << "\n"
    
    os
    << "format = " << kProjectFileFormatID_v1 << "\n"
    << "# This is a config file of Vst3SampleHost." << "\n"
    << "# The line starting '#' is treated as a comment line." << "\n"
    WRITE_MEMBER(sample_rate)
    WRITE_MEMBER(block_size)
    ;
    
#undef WRITE_MEMBER
    
    return os;
}

std::istream & operator>>(std::istream &is, ProjectFile &self)
{
    is.exceptions(std::ios::badbit);
    
    auto const lines = get_lines(is);
    
    if(auto val = find_value(lines, "format")) {
        if(*val != kProjectFileFormatID_v1) {
            throw Config::FailedToParse("Unknown format.");
        }
    }
    
#define READ_MEMBER(key, func) \
if(auto val = find_value(lines, #key)) { self. key ## _ = func(*val); }
    
    READ_MEMBER(sample_rate, [](std::string const &str) {
        return Clamp<double>(stof_or(str, kSupportedSampleRateDefault),
                             kSupportedSampleRateMin,
                             kSupportedSampleRateMax);
    });
    
    READ_MEMBER(block_size, [](std::string const &str) {
        return Clamp<double>(stoi_or(str, kSupportedBlockSizeDefault),
                             kSupportedBlockSizeMin,
                             kSupportedBlockSizeMax);
    });
    
#undef READ_MEMBER
    
    return is;
}

NS_HWM_END
