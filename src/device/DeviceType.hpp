#pragma once

NS_HWM_BEGIN

enum class DeviceIOType {
    kInput,
    kOutput,
};

std::string to_string(DeviceIOType io);
std::wstring to_wstring(DeviceIOType io);

//! strをDeviceIOTypeに変換して返す。
//! strが "Input" でも "Output" でもないときはstd::nulloptを返す。
std::optional<DeviceIOType> to_device_io_type(std::string const &str);

//! strをDeviceIOTypeに変換して返す。
//! strが "Input" でも "Output" でもないときはstd::nulloptを返す。
std::optional<DeviceIOType> to_device_io_type(std::wstring const &str);

enum class AudioDriverType {
    kUnknown,
    kDirectSound,
    kMME,
    kASIO,
    kWDMKS,
    kWASAPI,
    kCoreAudio,
    kALSA,
    kJACK,
};

std::string to_string(AudioDriverType type);
std::wstring to_wstring(AudioDriverType type);

//! strをAudioDriverTypeに変換して返す。
//! strがドライバ名として解釈できなかったり、"Unknown"の場合はstd::nulloptを返す
std::optional<AudioDriverType> to_audio_driver_type(std::string const &str);

//! strをAudioDriverTypeに変換して返す。
//! strがドライバ名として解釈できなかったり、"Unknown"の場合はstd::nulloptを返す
std::optional<AudioDriverType> to_audio_driver_type(std::wstring const &str);

NS_HWM_END
