#include "DeviceType.hpp"
#include "../misc/StrCnv.hpp"
#include <map>

NS_HWM_BEGIN

std::map<DeviceIOType, std::string> const kDeviceIOTypeTable
{
    { DeviceIOType::kInput, "Input" },
    { DeviceIOType::kOutput, "Output" },
};

std::map<AudioDriverType, std::string> const kAudioDriverTypeTable
{
    { AudioDriverType::kUnknown, "Unknown" },
    { AudioDriverType::kDirectSound, "DirectSound" },
    { AudioDriverType::kMME, "MME" },
    { AudioDriverType::kASIO, "ASIO" },
    { AudioDriverType::kWDMKS, "WDMKS" },
    { AudioDriverType::kWASAPI, "WASAPI" },
    { AudioDriverType::kCoreAudio, "CoreAudio" },
    { AudioDriverType::kALSA, "ALSA" },
    { AudioDriverType::kJACK, "JACK" },
};

std::string to_string(DeviceIOType io)
{
    auto found = kDeviceIOTypeTable.find(io);
    assert(found != kDeviceIOTypeTable.end());
    
    return found->second;
}

std::wstring to_wstring(DeviceIOType io)
{
    return to_wstr(to_string(io));
}

std::optional<DeviceIOType> to_device_io_type(std::string const &str)
{
    for(auto const &entry: kDeviceIOTypeTable) {
        if(entry.second == str) { return entry.first; }
    }
    
    return std::nullopt;
}

std::optional<DeviceIOType> to_device_io_type(std::wstring const &str)
{
    return to_device_io_type(to_utf8(str));
}

std::string to_string(AudioDriverType driver)
{
    auto found = kAudioDriverTypeTable.find(driver);
    assert(found != kAudioDriverTypeTable.end());
    
    return found->second;
}

std::wstring to_wstring(AudioDriverType driver)
{
    return to_wstr(to_string(driver));
}

std::optional<AudioDriverType> to_audio_driver_type(std::string const &str)
{
    if(str == kAudioDriverTypeTable.find(AudioDriverType::kUnknown)->second) {
        return std::nullopt;
    }
    
    for(auto const &entry: kAudioDriverTypeTable) {
        if(entry.second == str) { return entry.first; }
    }
    
    return std::nullopt;
}

std::optional<AudioDriverType> to_audio_driver_type(std::wstring const &str)
{
    return to_audio_driver_type(to_utf8(str));
}

NS_HWM_END
