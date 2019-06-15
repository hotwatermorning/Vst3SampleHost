#pragma once

#include <iostream>
#include "../device/DeviceType.hpp"

NS_HWM_BEGIN

//! このアプリケーションで使用するコンフィグデータのクラス
struct ProjectFile
{
    double sample_rate_ = kSupportedSampleRateDefault;
    Int32 block_size_ = kSupportedBlockSizeDefault;
    
    //! 現在のオーディオデバイスの状態を読み込み
    void ScanAudioDeviceStatus();
    
    //! ostreamにコンフィグデータを書き出し
    friend
    std::ostream & operator<<(std::ostream &os, ProjectFile const &self);
    
    class FailedToParse : public std::ios_base::failure
    {
    public:
        FailedToParse(std::string const &error_msg);
    };
    
    //! istreamからコンフィグデータを読み込む。
    /*! @exception FailedToParse
     */
    friend
    std::istream & operator>>(std::istream &is, ProjectFile &self);
};

NS_HWM_END
