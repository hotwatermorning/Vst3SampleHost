#include <algorithm>
#include <fstream>

#include <wx/filename.h>
#include <wx/cmdline.h>

#include "App.hpp"
#include "../device/AudioDeviceManager.hpp"
#include "../device/MidiDeviceManager.hpp"
#include "../plugin/vst3/Vst3Plugin.hpp"
#include "../plugin/vst3/Vst3PluginFactory.hpp"
#include "../misc/StrCnv.hpp"
#include "../misc/MathUtil.hpp"
#include "../misc/TransitionalVolume.hpp"
#include "../misc/LockFactory.hpp"
#include "../misc/Algorithm.hpp"
#include "../resource/ResourceHelper.hpp"
#include "../gui/Gui.hpp"
#include "../gui/PCKeyboardInput.hpp"
#include "../gui/DeviceSettingDialog.hpp"
#include "../gui/PluginEditor.hpp"
#include "../gui/AboutDialog.hpp"
#include "../processor/EventBuffer.hpp"
#include "../file/Config.hpp"
#include "../file/ProjectFile.hpp"
#include "../log/LoggingSupport.hpp"
#include "../log/LoggingStrategy.hpp"
#include "./NoteStatus.hpp"
#include "./TestSynth.hpp"

NS_HWM_BEGIN

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
    
    HWM_INFO_LOG(L"Enumerate Audio Devices");
    auto audio_device_infos = adm->Enumerate();
    for(auto const &info: audio_device_infos) {
        HWM_INFO_LOG(info.name_ << L" - " << to_wstring(info.driver_) << L"(" << info.num_channels_ << L"ch)");
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
        HWM_INFO_LOG(L"audio output device not found: " << conf.audio_output_device_name_);
        return false;
    }
    
    auto result = adm->Open(input_device, output_device, conf.sample_rate_, conf.block_size_);
    if(result.is_right() == false) {
        HWM_ERROR_LOG(L"failed to open the audio output device: " + result.left().error_msg_ << std::endl);
        return false;
    }
    
    return true;
}

std::vector<IMidiDevice *> OpenMidiDevices()
{
    auto mdm = MidiDeviceManager::GetInstance();
    
    std::vector<IMidiDevice *> list;
    
    HWM_INFO_LOG(L"Enumerate Midi Devices");
    auto midi_device_infos = mdm->Enumerate();
    for(auto const &info: midi_device_infos) {
        HWM_INFO_LOG(wxString::Format(L"[%6ls] %ls - ", to_wstring(info.io_type_), info.name_id_).ToStdWstring());
    }
    
    // midi 入力デバイスは全て開く
    for(auto const &info: midi_device_infos) {
        if(info.io_type_ == DeviceIOType::kInput) {
            String error;
            auto dev = mdm->Open(info, &error);
            if(!dev) {
                HWM_ERROR_LOG(wxString::Format(L"Failed to open [%ls]: %ls", info.name_id_, error).ToStdWstring());
            } else {
                list.push_back(dev);
                HWM_INFO_LOG(wxString::Format(L"Opened [%ls]", info.name_id_).ToStdWstring());
            }
        } else {
            HWM_INFO_LOG(wxString::Format(L"Skipped [%ls]", info.name_id_).ToStdWstring());
        }
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
    std::vector<IMidiDevice *> midi_ins_; //!< オープンしたMIDI入力デバイス
    std::vector<DeviceMidiMessage> device_midi_messages_;
    wxFrame *frame_;
    std::shared_ptr<Vst3PluginFactoryList> factory_list_;
    std::shared_ptr<Vst3PluginFactory> factory_;
    std::shared_ptr<Vst3Plugin> plugin_;
    
    ListenerService<IModuleLoadListener> mlls_;
    ListenerService<IPluginLoadListener> plls_;
    ListenerService<IPlaybackOptionChangeListener> pocls_;
    TestSynth test_synth_;
    IAboutDialog *about_dialog_ = nullptr;
    
    class Result {
    public:
        Result() {}
        
        static
        Result NoError() { return Result{}; }
        
        static
        Result Fail(String error_msg) {
            Result tmp;
            tmp.error_msg_ = std::move(error_msg);
            return tmp;
        }
        
        bool has_error() const { return error_msg_.empty() == false; }
        String const & what() const { return error_msg_; }
        
    private:
        String error_msg_;
    };
    
    Result ReadConfigFile()
    {
        wxFileName conf_path(GetConfigFilePath());
        if(conf_path.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) == false) {
            HWM_ERROR_LOG(L"failed to create the config file directory");
            return Result::Fail(L"コンフィグファイルを配置するディレクトリを作成できませんでした");
        }
    
        errno = 0;
#if defined(_MSC_VER)
        std::ifstream ifs(conf_path.GetFullPath().ToStdWstring());
#else
        std::ifstream ifs(to_utf8(conf_path.GetFullPath().ToStdWstring()));
#endif
        if(!ifs) {
            auto msg = to_wstr(strerror(errno));
            HWM_ERROR_LOG(L"failed to open the config file: " + msg);
            return Result::Fail(L"コンフィグファイルを開けませんでした: " + msg);
        }
        
        try {
            ifs >> config_;
        } catch(std::exception &e) {
            // エラー扱いにしない。
            // アプリケーション終了時に新しい内容が書き込まれるはず。
            HWM_WARN_LOG(L"Failed to read the config file: " << to_wstr(e.what()));
        }
        
        return Result::NoError();
    }
    
    Result WriteConfigFile()
    {
        wxFileName conf_path(GetConfigFilePath());
        if(conf_path.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) == false) {
            HWM_ERROR_LOG(L"failed to create the config file directory");
            return Result::Fail(L"コンフィグファイルを配置するディレクトリを作成できませんでした");
        }
        
        errno = 0;
#if defined(_MSC_VER)
        std::ofstream ofs(conf_path.GetFullPath().ToStdWstring(), std::ios::trunc);
#else
        std::ofstream ofs(to_utf8(conf_path.GetFullPath().ToStdWstring()), std::ios::trunc);
#endif
        if(!ofs) {
            auto msg = to_wstr(strerror(errno));
            HWM_ERROR_LOG(L"failed to open the config file: " + msg);
            return Result::Fail(L"コンフィグファイルのオープンに失敗しました: " + msg);
        }
        
        errno = 0;
        ofs << config_;
        if(!ofs) {
            auto msg = to_wstr(strerror(errno));
            HWM_ERROR_LOG(L"failed to write the config file: " + msg);
            return Result::Fail(L"コンフィグファイルの書き込みに失敗しました: " + msg);
        }
        
        return Result::NoError();
    }
    
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
        
        // App内部では、モノラル入力も必ずステレオにして扱う
        input_buffer_.resize(std::max(num_input_channels, 2), max_block_size);
        output_buffer_.resize(std::max(num_output_channels, 2), max_block_size);
        
        output_level_ = TransitionalVolume(sample_rate_,
                                           kAudioOutputLevelTransientMillisec,
                                           kAudioOutputLevelMinDB,
                                           kAudioOutputLevelMaxDB);
        output_level_.set_target_db_immediately(-10.0);

        if(plugin_) {
            plugin_->SetSamplingRate(sample_rate_);
            plugin_->SetBlockSize(block_size_);
            plugin_->Resume();
        }
        
        test_synth_.SetSampleRate(sample_rate);
    }
    
    void ProcessMidiEvents(SampleCount block_size)
    {
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
        
        // playing_変数の更新はここでのみ行う。
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
    }
    
    void ProcessPlugin(SampleCount block_size, AudioSample **output)
    {
        ProcessInfo pi;
        
        pi.input_audio_buffer_ = BufferRef<AudioSample const>(input_buffer_, 0, input_buffer_.channels(), 0, block_size);
        pi.output_audio_buffer_ = BufferRef<AudioSample>(output_buffer_, 0, input_buffer_.channels(), 0, block_size);
        pi.time_info_.is_playing_ = true;
        pi.time_info_.sample_length_ = block_size;
        pi.time_info_.sample_rate_ = sample_rate_;
        pi.time_info_.sample_pos_ = continuous_sample_count_;
        pi.time_info_.ppq_pos_ = (continuous_sample_count_ / sample_rate_) * pi.time_info_.tempo_ / 60.0;
        continuous_sample_count_ += block_size;
        
        pi.input_event_buffers_ = &input_event_buffers_;
        pi.output_event_buffers_ = &output_event_buffers_;
        
        plugin_->Process(pi);
        
        int const num_po = plugin_->GetNumAudioOutputs();
        if(num_po >= 2 && num_output_channels_ == 1) {
            // mixdown stereo channels to mono
            auto const srcL = output_buffer_.data()[0];
            auto const srcR = output_buffer_.data()[1];
            auto dest = output[0];
            for(int smp = 0; smp < block_size; ++smp) {
                dest[smp] += (srcL[smp] + srcR[smp]) / 2.0;
            }
        } else if(num_po == 1 && num_output_channels_ >= 2) {
            // spread mono channel to stereo
            auto const src = output_buffer_.data()[0];
            auto destL = output[0];
            auto destR = output[1];
            for(int smp = 0; smp < block_size; ++smp) {
                destL[smp] = src[smp];
                destR[smp] = src[smp];
            }
        } else {
            auto const num_channels_to_copy = std::min(num_po, num_output_channels_);
            for(int ch = 0; ch < num_channels_to_copy; ++ch) {
                auto const src = output_buffer_.data()[ch];
                auto dest = output[ch];
                for(int smp = 0; smp < block_size; ++smp) {
                    dest[smp] = src[smp];
                }
            }
        }
        
        input_event_buffers_.Clear();
        output_event_buffers_.Clear();
    }
    
    void Process(SampleCount block_size,
                 float const * const * input,
                 float **output) override
    {
        auto lock = lf_playback_.make_lock();
        
        input_buffer_.fill(0.0);
        output_buffer_.fill(0.0);
        
        bool const use_dummy_synth = (!plugin_ || plugin_->GetComponentInfo().IsEffect());

        if(use_dummy_synth) {
            test_synth_.Process(input_buffer_.data()[0], block_size);
            std::copy_n(input_buffer_.data()[0], block_size, input_buffer_.data()[1]);
        }
        
        if(enable_audio_input_.load()) {
            auto ss = input;
            auto ds = input_buffer_.data();
            auto len = block_size;
            
            if(num_input_channels_ == 1) {
                add(ss[0], ss[0] + len, ds[0], ds[0] + len, ds[0]);
                add(ss[0], ss[0] + len, ds[1], ds[1] + len, ds[1]);
            } else {
                for(int ch = 0; ch < num_input_channels_; ++ch) {
                    add(ss[ch], ss[ch] + len, ds[ch], ds[ch] + len, ds[ch]);
                }
            }
        }
        
        ProcessMidiEvents(block_size);
        
        if(plugin_) {
            ProcessPlugin(block_size, output);
        } else {
            if(num_output_channels_ == 1) {
                auto const srcL = input_buffer_.data()[0];
                auto const srcR = input_buffer_.data()[1];
                auto dest = output[0];
                for(int smp = 0; smp < block_size; ++smp) {
                    dest[smp] += (srcL[smp] + srcR[smp]) / 2.0;
                }
            } else {
                auto const num_channels_to_copy = std::min<int>(input_buffer_.channels(), num_output_channels_);
                for(int ch = 0; ch < num_channels_to_copy; ++ch) {
                    std::copy_n(input_buffer_.data()[ch], block_size, output[ch]);
                }
            }
        }
        
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

    Buffer<AudioSample> input_buffer_;
    Buffer<AudioSample> output_buffer_;
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
{
    InitializeDefaultGlobalLogger();
}

App::~App()
{
}

bool App::OnInit()
{
    if(wxApp::OnInit() == false) {
        return false;
    }
    
    wxInitAllImageHandlers();
    
    auto logger = GetGlobalLogger();
    auto st = std::make_shared<FileLoggingStrategy>(GetLogFilePath());
    auto err = st->OpenPermanently();
    if(err) {
        wxMessageBox(L"Can't open log file: " + err.message());
        return false;
    }
    
    logger->SetStrategy(st);
    logger->StartLogging(true);
    
    EnableErrorCheckAssertionForLoggingMacros(true);
    
    HWM_INFO_LOG(L"Start " << kAppName << L" version " << kAppVersion << L" (" << kAppCommitID << L").");
    HWM_INFO_LOG(L"Start logging");
    
    pimpl_->factory_list_ = std::make_shared<Vst3PluginFactoryList>();

    auto adm = AudioDeviceManager::GetInstance();
    adm->AddCallback(pimpl_.get());

    auto res = pimpl_->ReadConfigFile();
    if(res.has_error()) {
        wxMessageBox(L"コンフィグファイルを開けませんでした。アプリケーションを終了します。\nError Message: [" + res.what() + L"]");
        logger->StartLogging(false);
        return false;
    }

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
    if(pimpl_->about_dialog_) {
        pimpl_->about_dialog_->Destroy();
    }
    
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
    
    HWM_INFO_LOG(L"End logging");
    
    return 0;
}

bool App::LoadVst3Module(String path)
{
    auto factory = pimpl_->factory_list_->FindOrCreateFactory(path);
    if(!factory) {
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

    std::unique_ptr<Vst3Plugin> tmp;
    try {
        tmp = factory->CreateByID(cid);
        assert(tmp);
    } catch(std::exception &e) {
        HWM_ERROR_LOG(L"Failed to create Vst3Plugin: " << to_wstr(e.what()));
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
        HWM_ERROR_LOG(L"Failed to setup Vst3Plugin buses: " << to_wstr(e.what()));
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
    
    pimpl_->requested_[note_number] = NoteStatus::CreateNoteOn((UInt8)velocity);
    pimpl_->test_synth_.keys_[note_number] = NoteStatus::CreateNoteOn((UInt8)velocity);
}

void App::SendNoteOff(Int32 note_number, int off_velocity)
{
    pimpl_->requested_[note_number] = NoteStatus::CreateNoteOff((UInt8)off_velocity);
    pimpl_->test_synth_.keys_[note_number] = NoteStatus::CreateNoteOff((UInt8)off_velocity);
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
    pimpl_->pocls_.Invoke([db](auto *li) {
        li->OnAudioOutputLevelChanged(db);
    });
}

void App::SetTestWaveformType(OscillatorType ot)
{
    pimpl_->test_synth_.SetOscillatorType(ot);
    pimpl_->pocls_.Invoke([ot](auto *li) {
        li->OnTestWaveformTypeChanged(ot);
    });
}

OscillatorType App::GetTestWaveformType() const
{
    return pimpl_->test_synth_.GetOscillatorType();
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
    auto res = pimpl_->WriteConfigFile();
    if(res.has_error()) {
        wxMessageBox(L"コンフィグデータの保存に失敗しました。アプリケーションを終了してください。\nError Message: [" + res.what() + L"]");
    }
    
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

void App::ShowAboutDialog()
{
    assert(pimpl_->about_dialog_ == nullptr);
    auto dlg = CreateAboutDialog();
 
    if(dlg->IsOk() == false) {
        wxMessageBox("Aboutダイアログの作成に失敗しました。", "Error", wxOK|wxCENTER|wxICON_ERROR);
        return;
    }

    pimpl_->about_dialog_ = dlg.release();
    pimpl_->about_dialog_->Bind(wxEVT_DESTROY, [this](auto &ev) {
        pimpl_->about_dialog_ = nullptr;
    });
    pimpl_->about_dialog_->Show();
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
        HWM_ERROR_LOG(L"failed to load project file [" << path_to_load << L"]: " << to_wstr(e.what()));
        return;
    }
    
    UnloadVst3Module();
    
    if(file.oscillator_type_) {
        SetTestWaveformType(*file.oscillator_type_);
    }
    
    SetAudioOutputLevel(file.audio_output_level_);
    EnableAudioInput(file.is_audio_input_enabled_);
    
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
    
    if(file.editor_type_) {
        auto frame = IMainFrame::GetInstance();
        wxCommandEvent ev(wxEVT_COMMAND_MENU_SELECTED);
        ev.SetId(IMainFrame::kID_View_PluginEditor);
        ev.SetEventObject(frame);
        frame->ProcessWindowEvent(ev);
        
        auto editor = IPluginEditorFrame::GetInstance();
        editor->SetViewType(*file.editor_type_);
    }
}

void App::SaveProjectFile(String path_to_save)
{
    ProjectFile file;
    file.ScanAudioDeviceStatus();
    file.ScanPluginStatus();
    file.ScanAppStatus();
    
    std::ofstream ofs;
#if defined(_MSC_VER)
    ofs.open(path_to_save);
#else
    ofs.open(to_utf8(path_to_save));
#endif
    
    ofs << file;
}

namespace {
    wxCmdLineEntryDesc const cmdline_descs [] =
    {
        { wxCMD_LINE_SWITCH, "h", "help", "show help", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
        { wxCMD_LINE_OPTION, "l", "logging-level", "set logging level to (Error|Warn|Info|Debug). the default value is \"Info\"", wxCMD_LINE_VAL_STRING, 0 },
        { wxCMD_LINE_NONE },
    };
}

void App::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.SetDesc(cmdline_descs);
    parser.SetSwitchChars("-");
}

bool App::OnCmdLineParsed(wxCmdLineParser& parser)
{
    auto logger = GetGlobalLogger();
    wxString level = "Info";
    parser.Found("l", &level);
    level = level.Capitalize();
    logger->SetMostDetailedActiveLoggingLevel(level.ToStdWstring());
    
    return true;
}

NS_HWM_END

#if !defined(ENABLE_BUILD_TESTS)
wxIMPLEMENT_APP(hwm::App);
#endif
