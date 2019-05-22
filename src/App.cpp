#include <algorithm>

#include "device/AudioDeviceManager.hpp"
#include "device/MidiDeviceManager.hpp"
#include "plugin/vst3/Vst3Plugin.hpp"
#include "plugin/vst3/Vst3PluginFactory.hpp"
#include "misc/StrCnv.hpp"
#include "misc/MathUtil.hpp"
#include "misc/TransitionalVolume.hpp"
#include "misc/LockFactory.hpp"
#include "App.hpp"
#include "gui/Gui.hpp"
#include "gui/PCKeyboardInput.hpp"
#include "processor/EventBuffer.hpp"

NS_HWM_BEGIN

SampleCount const kSampleRate = 44100;
SampleCount const kBlockSize = 256;
double const kAudioOutputLevelMinDB = -48.0;
double const kAudioOutputLevelMaxDB = 0.0;
Int32 kAudioOutputLevelTransientMillisec = 30;

void OpenAudioDevice()
{
    auto adm = AudioDeviceManager::GetInstance();
    
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
    
    auto output_device = find_entry(DeviceIOType::kOutput, 2, adm->GetDefaultDriver());
    if(!output_device) { output_device = find_entry(DeviceIOType::kOutput, 2); }
    
    if(!output_device) {
        throw std::runtime_error("No audio output devices found");
    }
    
    //! may not found
    auto input_device = find_entry(DeviceIOType::kInput, 2, output_device->driver_);
    
    auto result = adm->Open(input_device, output_device, kSampleRate, kBlockSize);
    if(result.is_right() == false) {
        throw std::runtime_error(to_utf8(L"Failed to open the device: " + result.left().error_msg_));
    }
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
    {}

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
{}

bool App::OnInit()
{
    if(wxApp::OnInit() == false) {
        return false;
    }
    
    wxInitAllImageHandlers();
    
    pimpl_->factory_list_ = std::make_shared<Vst3PluginFactoryList>();
    pimpl_->frame_ = CreateMainFrame();

    auto adm = AudioDeviceManager::GetInstance();
    adm->AddCallback(pimpl_.get());

    OpenAudioDevice();
    pimpl_->midi_ins_ = OpenMidiDevices();
    
    if(auto dev = adm->GetDevice()) {
        dev->Start();
    }
    
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

//! オーディオ入力を有効／無効にする
void App::EnableAudioInput(bool enable)
{
    pimpl_->enable_audio_input_.store(enable);
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

NS_HWM_END

#if !defined(ENABLE_BUILD_TESTS)
wxIMPLEMENT_APP(hwm::App);
#endif
