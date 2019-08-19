#include <string>
#include <regex>
#include <iomanip>

#include "./Util.hpp"
#include "./ProjectFile.hpp"
#include "../app/App.hpp"
#include "../misc/StringAlgo.hpp"
#include "../misc/MathUtil.hpp"
#include "../device/AudioDeviceManager.hpp"
#include "../plugin/vst3/Vst3PluginFactory.hpp"
#include "../gui/PluginEditor.hpp"

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

void ProjectFile::ScanPluginStatus()
{
    auto app = App::GetInstance();
    
    vst3_plugin_cid_.fill(0);
    vst3_plugin_path_.clear();
    vst3_plugin_proc_data_.clear();
    vst3_plugin_edit_data_.clear();
    editor_type_ = std::nullopt;
    
    auto factory = app->GetPluginFactory();
    if(!factory) {
        return;
    }
    
    auto fl = Vst3PluginFactoryList::GetInstance();
    vst3_plugin_path_ = fl->GetModulePath(factory);
    
    auto plugin = app->GetPlugin();
    if(!plugin) {
        return;
    }
    
    vst3_plugin_cid_ = plugin->GetComponentInfo().cid();
    
    auto dump_data = plugin->SaveData();
    if(!dump_data) { return; }
        
    vst3_plugin_proc_data_ = dump_data->processor_data_;
    vst3_plugin_edit_data_ = dump_data->edit_controller_data_;
    
    auto editor = IPluginEditorFrame::GetInstance();
    if(!editor) {
        // do nothing.
    } else {
        editor_type_ = editor->GetViewType();
    }
}

void ProjectFile::ScanAppStatus()
{
    oscillator_type_ = std::nullopt;
    audio_output_level_ = 0.0;
    is_audio_input_enabled_ = false;
    
    auto app = App::GetInstance();
    oscillator_type_ = app->GetTestWaveformType();
    audio_output_level_ = app->GetAudioOutputLevel();
    is_audio_input_enabled_ = app->IsAudioInputEnabled();
}

std::ostream & operator<<(std::ostream &os, ProjectFile const &self)
{
#define WRITE_MEMBER(name) \
<< write_line(#name, to_s(self. name ## _)) << "\n"
    
    os
    << "format = " << kProjectFileFormatID_v1 << "\n"
    << "# This is a config file of Vst3SampleHost." << "\n"
    << "# The line starting '#' is treated as a comment line." << "\n"
    WRITE_MEMBER(vst3_plugin_path)
    WRITE_MEMBER(vst3_plugin_cid)
    WRITE_MEMBER(vst3_plugin_proc_data)
    WRITE_MEMBER(vst3_plugin_edit_data)
    WRITE_MEMBER(editor_type)
    WRITE_MEMBER(sample_rate)
    WRITE_MEMBER(block_size)
    WRITE_MEMBER(oscillator_type)
    WRITE_MEMBER(audio_output_level)
    WRITE_MEMBER(is_audio_input_enabled)
    ;
    
#undef WRITE_MEMBER
    
    return os;
}

std::istream & operator>>(std::istream &is, ProjectFile &self)
{
    is.exceptions(std::ios::badbit);
    
    auto const lines = read_lines(is);
    
    if(auto val = find_value(lines, "format")) {
        if(*val != kProjectFileFormatID_v1) {
            throw Config::FailedToParse("Unknown format.");
        }
    }

#define READ_MEMBER(key) \
if(auto val = find_value(lines, #key)) { from_s(*val, self. key ## _); }
    
    READ_MEMBER(vst3_plugin_path);
    READ_MEMBER(vst3_plugin_cid);
    READ_MEMBER(vst3_plugin_proc_data);
    READ_MEMBER(vst3_plugin_edit_data);
    READ_MEMBER(editor_type);
    
    READ_MEMBER(sample_rate);
    self.sample_rate_ = Clamp<double>(self.sample_rate_,
                                      kSupportedSampleRateMin,
                                      kSupportedSampleRateMax);
    
    READ_MEMBER(block_size);
    self.block_size_ = Clamp<double>(self.block_size_,
                                     kSupportedBlockSizeMin,
                                     kSupportedBlockSizeMax);
    
    READ_MEMBER(oscillator_type);
    
    auto app = App::GetInstance();
    READ_MEMBER(audio_output_level);
    self.audio_output_level_ = Clamp<double>(self.audio_output_level_,
                                             app->GetAudioOutputMinLevel(),
                                             app->GetAudioOutputMaxLevel());
    
    READ_MEMBER(is_audio_input_enabled);
    
#undef READ_MEMBER
    
    return is;
}

NS_HWM_END
