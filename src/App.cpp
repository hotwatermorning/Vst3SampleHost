#include <wx/wx.h>

#include "device/AudioDeviceManager.hpp"
#include "plugin/vst3/Vst3Plugin.hpp"
#include "plugin/vst3/Vst3PluginFactory.hpp"
#include "plugin/PluginScanner.hpp"
#include "misc/StrCnv.hpp"
#include "App.hpp"
#include "gui/Gui.hpp"
#include "processor/EventBuffer.hpp"

NS_HWM_BEGIN

SampleCount kSampleRate = 44100;
SampleCount kBlockSize = 256;

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
    
    //! start the audio device.
    adm->GetDevice()->Start();
}

struct App::Impl
:   IAudioDeviceCallback
{
    Impl()
    {
        input_event_buffers_.SetNumBuffers(1);
        output_event_buffers_.SetNumBuffers(1);
        
        for(int i = 0; i < 128; ++i) {
            requested_[i] = NoteInfo { false, 0, 0, 0 };
            playing_[i] = NoteInfo { false, 0, 0, 0 };
        }
    }
    
    AudioDeviceManager adm_;
    wxFrame *frame_;
    std::shared_ptr<Vst3PluginFactoryList> factory_list_;
    std::shared_ptr<Vst3PluginFactory> factory_;
    std::shared_ptr<Vst3Plugin> plugin_;
    
    ListenerService<ModuleLoadListener> mlls_;
    ListenerService<PluginLoadListener> plls_;

    struct NoteInfo {
        bool is_note_on_;
        unsigned char channel_;
        unsigned char velocity_;
        unsigned char padding_; //!< 未使用。常に0であるべき
        
        bool operator==(NoteInfo const &rhs) const
        {
            auto to_tuple = [](auto const &self) {
                return std::tie(self.is_note_on_, self.channel_, self.velocity_, self.padding_);
            };
            return to_tuple(*this) == to_tuple(rhs);
        }
        
        bool operator!=(NoteInfo const &rhs) const
        {
            return !(*this == rhs);
        }
    };
    
    using NoteStatus = std::array<std::atomic<NoteInfo>, 128>;
    NoteStatus requested_; //!< 再生待ちのノート
    NoteStatus playing_;   //!< 再生中のノート
    
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
    }
    
    void Process(SampleCount block_size,
                 float const * const * input,
                 float **output) override
    {
        auto lock = lf_playback_.make_lock();
        
        if(!plugin_) { return; }
        
        ProcessInfo pi;
        pi.input_audio_buffer_ = BufferRef<AudioSample const>(
                                                              input, 0, num_input_channels_, 0, block_size
                                                              );
        pi.output_audio_buffer_ = BufferRef<AudioSample>(
                                                         output, 0, num_output_channels_, 0, block_size
                                                         );
        pi.time_info_.is_playing_ = true;
        pi.time_info_.sample_length_ = block_size;
        pi.time_info_.sample_rate_ = sample_rate_;
        pi.time_info_.sample_pos_ = continuous_sample_count_;
        pi.time_info_.ppq_pos_ = (continuous_sample_count_ / sample_rate_) * pi.time_info_.tempo_ / 60.0;

        for(int i = 0; i < 128; ++i) {
            auto cur = playing_[i].load();
            auto req = requested_[i].load();
            if(cur == req) { continue; }
            
            playing_[i].store(req); // playing_変数の更新はここでのみ行う。CASをしなくても問題ない。
            
            ProcessInfo::MidiMessage msg;
            if(req.is_note_on_) {
                assert(req.velocity_ != 0);
                msg.data_ = MidiDataType::NoteOn { (UInt8)i, req.velocity_ };
            } else {
                msg.data_ = MidiDataType::NoteOff { (UInt8)i, req.velocity_ };
            }
            input_event_buffers_.GetBuffer(0)->AddEvent(msg);
        }
        pi.input_event_buffers_ = &input_event_buffers_;
        pi.output_event_buffers_ = &output_event_buffers_;
        
        plugin_->Process(pi);
        
        input_event_buffers_.Clear();
        output_event_buffers_.Clear();
    }
    
    void StopProcessing() override
    {}
    
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
    
    auto list = adm->Enumerate();
    for(auto &device_info: list) {
        if(device_info.io_type_ == DeviceIOType::kOutput) {
            adm->Open(nullptr, &device_info, kSampleRate, kBlockSize);
            break;
        }
    }
    
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
    
    pimpl_->requested_[note_number] = Impl::NoteInfo { true, 0, (unsigned char)velocity, 0 };
}

void App::SendNoteOff(Int32 note_number, int off_velocity)
{
    pimpl_->requested_[note_number] = Impl::NoteInfo { false, 0, (unsigned char)off_velocity, 0 };
}

void App::StopAllNotes()
{
    for(int i = 0; i < 128; ++i) {
        SendNoteOff(i);
    }
}

std::bitset<128> App::GetPlayingNotes()
{
    std::bitset<128> ret;
    
    for(int i = 0; i < 128; ++i) {
        ret[i] = (pimpl_->playing_[i].load().velocity_ != 0);
    }
    
    return ret;
}

NS_HWM_END

#if !defined(TERRA_BUILD_TEST)
wxIMPLEMENT_APP(hwm::App);
#endif
