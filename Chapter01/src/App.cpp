#include <algorithm>
#include <fstream>

#include <wx/filename.h>

#include "device/AudioDeviceManager.hpp"
#include "device/MidiDeviceManager.hpp"
#include "misc/StrCnv.hpp"
#include "misc/MathUtil.hpp"
#include "misc/TransitionalVolume.hpp"
#include "misc/LockFactory.hpp"
#include "resource/ResourceHelper.hpp"
#include "App.hpp"
#include "gui/Gui.hpp"
#include "gui/PCKeyboardInput.hpp"
#include "gui/DeviceSettingDialog.hpp"
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
    }
    
    void Process(SampleCount block_size,
                 float const * const * input,
                 float **output) override
    {
        auto lock = lf_playback_.make_lock();
        
        // do nothing.
        return;
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
{
}

bool App::OnInit()
{
    if(wxApp::OnInit() == false) {
        return false;
    }
    
    wxInitAllImageHandlers();
    
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
    
    return 0;
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
}

void App::SaveProjectFile(String path_to_save)
{
    ProjectFile file;
    file.ScanAudioDeviceStatus();
    
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
