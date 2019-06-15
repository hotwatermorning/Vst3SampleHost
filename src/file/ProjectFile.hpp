#pragma once

#include <iostream>
#include "../device/DeviceType.hpp"

NS_HWM_BEGIN

//! このアプリケーションで使用するコンフィグデータのクラス
struct ProjectFile
{
    String vst3_plugin_path_;
    ClassInfo::CID vst3_plugin_cid_;
    std::vector<char> vst3_plugin_proc_data_;
    std::vector<char> vst3_plugin_edit_data_;
    
    String editor_type_; // L"generic" or L"dedicated" or L""
    
    double sample_rate_ = kSupportedSampleRateDefault;
    Int32 block_size_ = kSupportedBlockSizeDefault;
    
    //! 現在のオーディオデバイスの状態を読み込み
    void ScanAudioDeviceStatus();
    void ScanPluginStatus();
    
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
