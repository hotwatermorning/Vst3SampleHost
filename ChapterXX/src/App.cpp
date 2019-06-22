#include <algorithm>
#include <fstream>

#include <wx/filename.h>

#include "device/AudioDeviceManager.hpp"
#include "device/MidiDeviceManager.hpp"
#include "plugin/vst3/Vst3Plugin.hpp"
#include "plugin/vst3/Vst3PluginFactory.hpp"
#include "misc/StrCnv.hpp"
#include "misc/MathUtil.hpp"
#include "misc/TransitionalVolume.hpp"
#include "misc/LockFactory.hpp"
#include "resource/ResourceHelper.hpp"
#include "App.hpp"
#include "gui/Gui.hpp"
#include "gui/PCKeyboardInput.hpp"
#include "gui/DeviceSettingDialog.hpp"
#include "gui/PluginEditor.hpp"
#include "processor/EventBuffer.hpp"
#include "file/Config.hpp"
#include "file/ProjectFile.hpp"

NS_HWM_BEGIN

SampleCount const kSampleRate = 44100;
SampleCount const kBlockSize = 256;
double const kAudioOutputLevelMinDB = -48.0;
double const kAudioOutputLevelMaxDB = 0.0;
Int32 kAudioOutputLevelTransientMillisec = 30;

bool OpenAudioDevice(Config const &conf)
{
    if(conf.audio_output_device_name_.empty()) {
        return false;
    }
    
    auto adm = AudioDeviceManager::GetInstance();
    
    adm->Close();
    
    auto audio_device_infos = adm->Enumerate();
    for(auto const &info: audio_device_infos) {
        hwm::wdout << info.name_ << L" - " << to_wstring(info.driver_) << L"(" << info.num_channels_ << L"ch)" << std::endl;
    }
    
    auto find_entry = [&list = audio_device_infos](auto io_type,
                                                   auto min_channels,
                                                   std::optional<AudioDriverType> driver = std::nullopt,
                                                   std::optional<String> name = std::nullopt) -> AudioDeviceInfo const *
    {
        auto found = std::find_if(list.begin(), list.end(), [&](auto const &x) {
            if(x.io_type_ != io_type)           { return false; }
            if(name && name != x.name_)         { return false; }
            if(driver && driver != x.driver_)   { return false; }
            if(x.num_channels_ < min_channels)  { return false; }
            return true;
        });
        if(found == list.end()) { return nullptr; }
        else { return &*found; }
    };
    
    auto output_device = find_entry(DeviceIOType::kOutput,
                                    conf.audio_output_channel_count_,
                                    conf.audio_output_driver_type_,
                                    conf.audio_output_device_name_);

    //! may not found
    auto input_device = find_entry(DeviceIOType::kInput,
                                   conf.audio_input_channel_count_,
                                   conf.audio_input_driver_type_,
                                   conf.audio_input_device_name_);
    
    if(!output_device) {
        hwm::wdout << L"audio output device not found: " << conf.audio_output_device_name_ << std::endl;
        return false;
    }
    
    auto result = adm->Open(input_device, output_device, conf.sample_rate_, conf.block_size_);
    if(result.is_right() == false) {
        hwm::wdout << L"failed to open the audio output device: " + result.left().error_msg_ << std::endl;
        return false;
    }
    
    return true;
}

std::vector<MidiDevice *> OpenMidiDevices()
{
    auto mdm = MidiDeviceManager::GetInstance();
    
    std::vector<MidiDevice *> list;
    
    auto midi_device_infos = mdm->Enumerate();
    for(auto info: midi_device_infos) {
        hwm::wdout
        << wxString::Format(L"[%6ls] %ls - ", to_wstring(info.io_type_), info.name_id_
                            ).ToStdWstring();
        
        if(info.io_type_ == DeviceIOType::kInput) {
            String error;
            auto dev = mdm->Open(info, &error);
            if(!dev) {
                hwm::wdout << L"Failed to open: " << error;
            } else {
                list.push_back(dev);
                hwm::wdout << L"Opened.";
            }
        } else {
            hwm::wdout << "Skip.";
        }
        hwm::wdout << std::endl;
    }
    
    return list;
}

struct App::Impl
:   IAudioDeviceCallback
{
    Impl()
    {
        input_event_buffers_.SetNumBuffers(1);
        output_event_buffers_.SetNumBuffers(1);
        
        for(int i = 0; i < 128; ++i) {
            requested_[i] = NoteStatus::CreateNull();
            playing_[i] = NoteStatus::CreateNull();
        }
        
        enable_audio_input_.store(false);
    }
    
    Config config_;
    TransitionalVolume output_level_;
    PCKeyboardInput keyinput_;
    std::atomic<bool> enable_audio_input_ = { false };
    AudioDeviceManager adm_;
    MidiDeviceManager mdm_;
    std::vector<MidiDevice *> midi_ins_; //!< オープンしたMIDI入力デバイス
    std::vector<DeviceMidiMessage> device_midi_messages_;
    wxFrame *frame_;
    std::shared_ptr<Vst3PluginFactoryList> factory_list_;
    std::shared_ptr<Vst3PluginFactory> factory_;
    std::shared_ptr<Vst3Plugin> plugin_;
    
    ListenerService<ModuleLoadListener> mlls_;
    ListenerService<PluginLoadListener> plls_;
    ListenerService<PlaybackOptionChangeListener> pocls_;
    
    bool ReadConfigFile()
    {
        wxFileName conf_path(GetConfigFilePath());
        if(conf_path.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) == false) {
            hwm::dout << "Failed to open the config file." << std::endl;
            return false;
        }
        
#if defined(_MSC_VER)
        std::ifstream ifs(conf_path.GetFullPath().ToStdWstring());
#else
        std::ifstream ifs(to_utf8(conf_path.GetFullPath().ToStdWstring()));
#endif
        if(!ifs) {
            hwm::dout << "Failed to open the config file." << std::endl;
            return false;
        }
        
        try {
            ifs >> config_;
            return true;
        } catch(std::exception &e) {
            hwm::dout << "Failed to read the config file: " << e.what() << std::endl;
            return false;
        }
    }
    
    void WriteConfigFile()
    {
        try {
            wxFileName conf_path(GetConfigFilePath());
            if(conf_path.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) == false) {
                throw std::runtime_error("Failed to create the config directory.");
            }
            
            std::ofstream ofs;
            ofs.exceptions(std::ios::failbit|std::ios::badbit);
        
#if defined(_MSC_VER)
            ofs.open(conf_path.GetFullPath().ToStdWstring());
#else
            ofs.open(to_utf8(conf_path.GetFullPath().ToStdWstring()));
#endif
            ofs << config_;
        } catch(std::exception &e) {
            hwm::dout << "Failed to write config data: " << e.what() << std::endl;
            auto msg = std::string("コンフィグファイルの書き込みに失敗しました。\nアプリケーションを終了します。\n[") + e.what() + "]";
            wxMessageBox(msg);
            std::exit(-1);
        }
    }

    struct alignas(4) NoteStatus {
        enum Type : char {
            kNull,
            kNoteOn,
            kNoteOff,
        };
        
        static NoteStatus CreateNoteOn(UInt8 velocity)
        {
            assert(velocity != 0);
            return NoteStatus { kNoteOn, velocity, 0 };
        }
        
        static NoteStatus CreateNoteOff(UInt8 off_velocity)
        {
            return NoteStatus { kNoteOff, off_velocity, 0 };
        }
        
        static NoteStatus CreateNull()
        {
            return NoteStatus {};
        }
        
        bool is_null() const { return type_ == kNull; }
        bool is_note_on() const { return type_ == kNoteOn; }
        bool is_note_off() const { return type_ == kNoteOff; }
        UInt8 get_velocity() const { return velocity_; }
        
        bool operator==(NoteStatus const &rhs) const
        {
            auto to_tuple = [](auto const &self) {
                return std::tie(self.type_, self.velocity_);
            };
            
            return to_tuple(*this) == to_tuple(rhs);
        }
        
        bool operator!=(NoteStatus const &rhs) const
        {
            return !(*this == rhs);
        }

        Type type_ = Type::kNull;
        UInt8 velocity_ = 0;
        UInt16 dummy_padding_ = 0;
    };
    
    using KeyboardStatus = std::array<std::atomic<NoteStatus>, 128>;
    KeyboardStatus requested_; //!< 再生待ちのノート
    KeyboardStatus playing_;   //!< 再生中のノート
    
    void StartProcessing(double sample_rate,
                         SampleCount max_block_size,
                         int num_input_channels,
                         int num_output_channels) override
    {
        num_input_channels_ = num_input_channels;
        num_output_channels_ = num_output_channels;
        continuous_sample_count_ = 0;
        block_size_ = max_block_size;
        sample_rate_ = sample_rate;
        
        silent_.resize(num_input_channels, max_block_size);
        silent_.fill(0.0);
        
        output_level_ = TransitionalVolume(sample_rate_,
                                           kAudioOutputLevelTransientMillisec,
                                           kAudioOutputLevelMinDB,
                                           kAudioOutputLevelMaxDB);

        if(plugin_) {
            plugin_->SetSamplingRate(sample_rate_);
            plugin_->SetBlockSize(block_size_);
            plugin_->Resume();
        }
    }
    
    void Process(SampleCount block_size,
                 float const * const * input,
                 float **output) override
    {
        auto lock = lf_playback_.make_lock();
        
        if(!plugin_) { return; }
        
        auto const input_buf = enable_audio_input_.load() ? input : silent_.data();

        ProcessInfo pi;
        
        pi.input_audio_buffer_ = BufferRef<AudioSample const>(input_buf, 0, num_input_channels_, 0, block_size);
        pi.output_audio_buffer_ = BufferRef<AudioSample>(output, 0, num_output_channels_, 0, block_size);
        pi.time_info_.is_playing_ = true;
        pi.time_info_.sample_length_ = block_size;
        pi.time_info_.sample_rate_ = sample_rate_;
        pi.time_info_.sample_pos_ = continuous_sample_count_;
        pi.time_info_.ppq_pos_ = (continuous_sample_count_ / sample_rate_) * pi.time_info_.tempo_ / 60.0;
        continuous_sample_count_ += block_size;
        
        assert(input_event_buffers_.GetNumBuffers() >= 1);
        auto &buf0 = *input_event_buffers_.GetBuffer(0);
        for(UInt8 i = 0; i < 128; ++i) {
            auto req = requested_[i].exchange(NoteStatus::CreateNull());
            if(req.is_null()) { continue; }
            
            ProcessInfo::MidiMessage msg;
            if(req.is_note_on()) {
                msg.data_ = MidiDataType::NoteOn { i, req.get_velocity() };
            } else {
                msg.data_ = MidiDataType::NoteOff { i, req.get_velocity() };
            }
            buf0.AddEvent(msg);
        }
        
        auto mdm = MidiDeviceManager::GetInstance();
        double const now = mdm->GetMessages(device_midi_messages_);
        
        double const frame_begin_sec = now - (block_size / sample_rate_);
        for(auto const &dev_msg: device_midi_messages_) {
            ProcessInfo::MidiMessage msg;
            msg.data_ = dev_msg.data_;
            msg.offset_ = std::round(dev_msg.time_stamp_ - frame_begin_sec) * sample_rate_;
            buf0.AddEvent(msg);
        }
        buf0.Sort();
        
        // playing_変数の更新はここでのみ行う。CASをしなくても問題ない。
        for(int i = 0; i < buf0.GetCount(); ++i) {
            auto const &ev = buf0.GetEvent(i);
            if(auto p = ev.As<MidiDataType::NoteOn>()) {
                assert(p->pitch_ < playing_.size());
                playing_[p->pitch_].store(NoteStatus::CreateNoteOn(p->velocity_));
            } else if(auto p = ev.As<MidiDataType::NoteOff>()) {
                assert(p->pitch_ < playing_.size());
                playing_[p->pitch_].store(NoteStatus::CreateNoteOff(p->off_velocity_));
            }
        }
        
        pi.input_event_buffers_ = &input_event_buffers_;
        pi.output_event_buffers_ = &output_event_buffers_;
        
        plugin_->Process(pi);
        
        input_event_buffers_.Clear();
        output_event_buffers_.Clear();
        
        output_level_.update_transition(block_size);
        double const gain = output_level_.get_current_linear_gain();
        
        for(Int32 ch = 0; ch < num_output_channels_; ++ch) {
            auto ch_data = output[ch];
            std::for_each_n(ch_data, block_size,
                            [gain](auto &elem) { elem *= gain; }
                            );
        }
    }
    
    void StopProcessing() override
    {
        if(plugin_) {
            plugin_->Suspend();
        }
    }

    Buffer<AudioSample> silent_;
    int num_input_channels_ = 0;
    int num_output_channels_ = 0;
    int block_size_ = 0;
    int continuous_sample_count_ = 0;
    double sample_rate_ = 0;
    LockFactory lf_playback_;
    EventBufferList input_event_buffers_;
    EventBufferList output_event_buffers_;
};

App::App()
:   pimpl_(std::make_unique<Impl>())
{}

App::~App()
{
}

bool App::OnInit()
{
    if(wxApp::OnInit() == false) {
        return false;
    }
    
    wxInitAllImageHandlers();
    
    pimpl_->factory_list_ = std::make_shared<Vst3PluginFactoryList>();

    auto adm = AudioDeviceManager::GetInstance();
    adm->AddCallback(pimpl_.get());

    pimpl_->ReadConfigFile();

    if(OpenAudioDevice(pimpl_->config_) == false) {
        // Select Audio Device
        SelectAudioDevice();
    }
    
    if(adm->GetDevice() == nullptr) {
        wxMessageBox(L"利用できるオーディオ出力デバイスがありません。");
        return false;
    }
    
    pimpl_->midi_ins_ = OpenMidiDevices();
    
    if(auto dev = adm->GetDevice()) {
        dev->Start();
    }
    
    pimpl_->frame_ = CreateMainFrame();
    pimpl_->frame_->CentreOnScreen();
    pimpl_->frame_->Layout();
    pimpl_->frame_->Show(true);
    pimpl_->frame_->SetFocus();
    
    return true;
}

int App::OnExit()
{
    auto adm = AudioDeviceManager::GetInstance();
    auto mdm = MidiDeviceManager::GetInstance();
    
    for(auto dev: pimpl_->midi_ins_) {
        mdm->Close(dev);
    }
    
    if(auto *dev = adm->GetDevice()) {
        dev->Stop();
        adm->Close();
    }
    
    adm->RemoveCallback(pimpl_.get());
    
    UnloadVst3Module();
    
    pimpl_->factory_list_.reset();
    
    return 0;
}

bool App::LoadVst3Module(String path)
{
    auto factory = pimpl_->factory_list_->FindOrCreateFactory(path);
    if(!factory) {
        hwm::dout << "Failed to open module.";
        return false;
    }
    
    if(factory != pimpl_->factory_) {
        UnloadVst3Module();
    }
    
    pimpl_->factory_ = std::move(factory);
    
    pimpl_->mlls_.Invoke([path, factory = pimpl_->factory_.get()](auto *listener) {
        listener->OnAfterModuleLoaded(path, factory);
    });
    return true;
}

void App::UnloadVst3Module()
{
    if(!pimpl_->factory_) { return; }
    
    UnloadVst3Plugin(); // 開いているプラグインがあれば閉じる
    
    pimpl_->mlls_.Invoke([factory = pimpl_->factory_.get()](auto *listener) {
        listener->OnBeforeModuleUnloaded(factory);
    });
    
    pimpl_->factory_.reset();
}

bool App::LoadVst3Plugin(ClassInfo::CID cid)
{
    auto factory = GetPluginFactory();
    if(!factory) { return false; }

    auto tmp = factory->CreateByID(cid);
    if(!tmp) {
        return false;
    }
    
    UnloadVst3Plugin();
    
    tmp->SetSamplingRate(pimpl_->sample_rate_);
    tmp->SetBlockSize(pimpl_->block_size_);
    
    try {
        auto activate_all_buses = [](Vst3Plugin *plugin,
                                     Steinberg::Vst::MediaTypes media,
                                     Steinberg::Vst::BusDirections dir)
        {
            auto const num = plugin->GetNumBuses(media, dir);
            for(int i = 0; i < num; ++i) { plugin->SetBusActive(media, dir, i); }
        };
        
        using MT = Steinberg::Vst::MediaTypes;
        using BD = Steinberg::Vst::BusDirections;
        
        activate_all_buses(tmp.get(), MT::kAudio, BD::kInput);
        activate_all_buses(tmp.get(), MT::kAudio, BD::kOutput);
        activate_all_buses(tmp.get(), MT::kEvent, BD::kInput);
        activate_all_buses(tmp.get(), MT::kEvent, BD::kOutput);
        
    } catch(std::exception &e) {
        hwm::dout << "Failed to create a Vst3Plugin: " << e.what() << std::endl;
        return nullptr;
    }
    
    tmp->SetSamplingRate(pimpl_->sample_rate_);
    tmp->SetBlockSize(pimpl_->block_size_);
    tmp->Resume();
    
    {
        auto lock = pimpl_->lf_playback_.make_lock();
        pimpl_->plugin_ = std::move(tmp);
    }
    
    pimpl_->plls_.Invoke([plugin = pimpl_->plugin_.get()](auto *listener) {
        listener->OnAfterPluginLoaded(plugin);
    });
    
    return true;
}

void App::UnloadVst3Plugin()
{
    if(!pimpl_->plugin_) { return; }
    
    pimpl_->plls_.Invoke([plugin = pimpl_->plugin_.get()](auto *listener) {
        listener->OnBeforePluginUnloaded(plugin);
    });
    
    std::shared_ptr<Vst3Plugin> tmp;
    {
        auto lock = pimpl_->lf_playback_.make_lock();
        tmp = std::move(pimpl_->plugin_);
    }
    
    tmp->Suspend();
}

Vst3PluginFactory * App::GetPluginFactory()
{
    return pimpl_->factory_.get();
}

Vst3Plugin * App::GetPlugin()
{
    return pimpl_->plugin_.get();
}

App::ModuleLoadListenerService & App::GetModuleLoadListenerService()
{
    return pimpl_->mlls_;
}

App::PluginLoadListenerService & App::GetPluginLoadListenerService()
{
    return pimpl_->plls_;
}

App::PlaybackOptionChangeListenerService & App::GetPlaybackOptionChangeListenerService()
{
    return pimpl_->pocls_;
}

void App::SendNoteOn(Int32 note_number, Int32 velocity)
{
    assert(0 <= note_number && note_number < 128);
    if(velocity == 0) {
        SendNoteOff(note_number, 64);
        return;
    }
    
    pimpl_->requested_[note_number] = Impl::NoteStatus::CreateNoteOn((UInt8)velocity);
}

void App::SendNoteOff(Int32 note_number, int off_velocity)
{
    pimpl_->requested_[note_number] = Impl::NoteStatus::CreateNoteOff((UInt8)off_velocity);
}

void App::StopAllNotes()
{
    for(int i = 0; i < 128; ++i) {
        SendNoteOff(i);
    }
}

//! オーディオ入力が有効かどうか
bool App::IsAudioInputEnabled() const
{
    return pimpl_->enable_audio_input_.load();
}

bool App::CanEnableAudioInput() const
{
    auto adm = AudioDeviceManager::GetInstance();
    if(auto dev = adm->GetDevice()) {
        auto in_info = dev->GetDeviceInfo(DeviceIOType::kInput);
        assert(!in_info || in_info->num_channels_ > 0);
        
        return in_info != nullptr;
    }
    
    return false;
}

//! オーディオ入力を有効／無効にする
void App::EnableAudioInput(bool enable)
{
    if(enable == IsAudioInputEnabled()) { return; }
    
    pimpl_->enable_audio_input_.store(enable);
    pimpl_->pocls_.Invoke([enable](auto *li) {
        li->OnAudioInputEnableStateChanged(enable);
    });
}

double App::GetAudioOutputMinLevel() const
{
    return pimpl_->output_level_.get_min_db();
}

double App::GetAudioOutputMaxLevel() const
{
    return pimpl_->output_level_.get_max_db();
}

double App::GetAudioOutputLevel() const
{
    return pimpl_->output_level_.get_target_db();
}

void App::SetAudioOutputLevel(double db)
{
    pimpl_->output_level_.set_target_db(db);
}

std::bitset<128> App::GetPlayingNotes()
{
    std::bitset<128> ret;
    
    for(int i = 0; i < 128; ++i) {
        auto note = pimpl_->playing_[i].load();
        ret[i] = note.is_note_on(); // 現在はベロシティ情報は使用していない
    }
    
    return ret;
}

void App::SelectAudioDevice()
{
    bool const old_inputtability = CanEnableAudioInput();
    
    for( ; ; ) {
        auto dlg = CreateDeviceSettingDialog(nullptr);
        dlg->CenterOnScreen();
        dlg->ShowModal();
        dlg->Destroy();

        auto adm = AudioDeviceManager::GetInstance();
        
        if(auto dev = adm->GetDevice()) {
            dev->Start();
            break;
        }
        
        auto msg_dlg = new wxMessageDialog(nullptr, "", "", wxYES_NO|wxCENTRE);
        msg_dlg->SetMessage("オーディオ出力デバイスをオープンできませんでした。再試行しますか？");
        msg_dlg->SetTitle("再試行の確認");
        msg_dlg->SetYesNoLabels("再試行", "アプリの終了");
        auto result = msg_dlg->ShowModal();
        if(result != wxYES) {
            std::exit(-2);
        }
    }
    
    pimpl_->config_.ScanAudioDeviceStatus();
    pimpl_->WriteConfigFile();
    
    bool const new_inputtability = CanEnableAudioInput();
    if(old_inputtability != new_inputtability) {
        if(new_inputtability == false && IsAudioInputEnabled()) {
            EnableAudioInput(false);
        }
        
        pimpl_->pocls_.Invoke([new_inputtability](auto *li) {
            li->OnAudioInputAvailabilityChanged(new_inputtability);
        });
    }
}

Config & App::GetConfig()
{
    return pimpl_->config_;
}

Config const & App::GetConfig() const
{
    return pimpl_->config_;
}

void App::LoadProjectFile(String path_to_load)
{
    ProjectFile file;
    
    std::ifstream ifs;
#if defined(_MSC_VER)
    ifs.open(path_to_load);
#else
    ifs.open(to_utf8(path_to_load));
#endif
    
    try {
        ifs >> file;
    } catch(std::exception &e) {
        hwm::dout << "failed to load project file: " << e.what() << std::endl;
    }
    
    if(file.vst3_plugin_path_.empty()) { return; }
    
    if(!LoadVst3Module(file.vst3_plugin_path_)) {
        return;
    }
    
    if(!LoadVst3Plugin(file.vst3_plugin_cid_)) {
        return;
    }
    
    if(file.vst3_plugin_proc_data_.empty()) {
        return;
    }
    
    Vst3Plugin::DumpData dump;
    dump.processor_data_ = file.vst3_plugin_proc_data_;
    dump.edit_controller_data_ = file.vst3_plugin_edit_data_;
    
    GetPlugin()->LoadData(dump);
    
    if(file.editor_type_.empty() == false) {
        auto frame = IMainFrame::GetInstance();
        wxCommandEvent ev(wxEVT_COMMAND_MENU_SELECTED);
        ev.SetId(IMainFrame::kID_View_PluginEditor);
        ev.SetEventObject(frame);
        frame->ProcessWindowEvent(ev);
        
        using VT = IPluginEditorFrame::ViewType;
        auto editor = IPluginEditorFrame::GetInstance();
        auto const vt = (file.editor_type_ == L"generic" ? VT::kGeneric : VT::kDedicated);
        editor->SetViewType(vt);
    }
}

void App::SaveProjectFile(String path_to_save)
{
    ProjectFile file;
    file.ScanAudioDeviceStatus();
    file.ScanPluginStatus();
    
    std::ofstream ofs;
#if defined(_MSC_VER)
    ofs.open(path_to_save);
#else
    ofs.open(to_utf8(path_to_save));
#endif
    
    ofs << file;
}

NS_HWM_END

#if !defined(ENABLE_BUILD_TESTS)
wxIMPLEMENT_APP(hwm::App);
#endif
