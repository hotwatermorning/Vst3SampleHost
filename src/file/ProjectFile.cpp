#include <string>
#include <regex>
#include <iomanip>

#include "./Util.hpp"
#include "./ProjectFile.hpp"
#include "../App.hpp"
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
    editor_type_ = L"";
    
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
        auto vt = editor->GetViewType();
        if(vt == IPluginEditorFrame::ViewType::kDedicated) {
            editor_type_ = L"dedicated";
        } else {
            editor_type_ = L"generic";
        }
    }
}

std::ostream & operator<<(std::ostream &os, ProjectFile const &self)
{
#define WRITE_MEMBER(name) \
<< (#name + std::string(" = ")) << std::quoted(to_s(self. name ## _)) << "\n"
    
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
    
    READ_MEMBER(vst3_plugin_path, [](std::string const &str) {
        return to_wstr(str);
    });
    
    READ_MEMBER(vst3_plugin_cid, [](std::string const &str) {
        auto buf = base64_decode(str);
        
        ClassInfo::CID cid;
        if(buf.size() == cid.size()) {
            std::copy_n(buf.data(), cid.size(), cid.data());
        }
        
        return cid;
    });
    
    READ_MEMBER(vst3_plugin_proc_data, [](std::string const &str) {
        return std::vector<char>(base64_decode(str));
    });
    
    READ_MEMBER(vst3_plugin_edit_data, [](std::string const &str) {
        return std::vector<char>(base64_decode(str));
    });
    
    READ_MEMBER(editor_type, [](std::string const &str) {
        if(str == "generic" || str == "dedicated") {
            return to_wstr(str);
        } else {
            return String();
        }
    });
    
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
