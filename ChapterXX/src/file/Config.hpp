#pragma once

#include <iostream>
#include "../device/DeviceType.hpp"

NS_HWM_BEGIN

//! このアプリケーションで使用するコンフィグデータのクラス
struct Config
{
    AudioDriverType audio_input_driver_type_ = AudioDriverType::kUnknown;
    String audio_input_device_name_;
    Int32 audio_input_channel_count_ = 0;
    
    AudioDriverType audio_output_driver_type_ = AudioDriverType::kUnknown;
    String audio_output_device_name_;
    Int32 audio_output_channel_count_ = 0;
    
    double sample_rate_ = kSupportedSampleRateDefault;
    Int32 block_size_ = kSupportedBlockSizeDefault;
    
    //! 現在のオーディオデバイスの状態を読み込み
    void ScanAudioDeviceStatus();
    
    //! ostreamにコンフィグデータを書き出し
    friend
    std::ostream & operator<<(std::ostream &os, Config const &self);

    class FailedToParse : public std::ios_base::failure
    {
    public:
        FailedToParse(std::string const &error_msg);
    };
    
    //! istreamからコンフィグデータを読み込む。
    /*! @exception FailedToParse
     */
    friend
    std::istream & operator>>(std::istream &is, Config &self);
};

NS_HWM_END
