#include <algorithm>
#include <fstream>

#include <wx/filename.h>

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

NS_HWM_BEGIN

double const kAudioOutputLevelMinDB = -48.0;
double const kAudioOutputLevelMaxDB = 0.0;
Int32 kAudioOutputLevelTransientMillisec = 30;

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

struct TestSynth
{
    struct WaveTable
    {
        WaveTable(int length_as_shift_size)
        {
            assert(length_as_shift_size <= 31);

            shift_ = length_as_shift_size;
            wave_data_.resize(1 << shift_);
        }
        
        //! Get the table value of the specified position.
        //! @param pos is a normalized pos [0.0 - 1.0)
        AudioSample GetValueSimple(double pos) const
        {
            auto const k = 1 << shift_;
            auto const sample_pos = (SampleCount)std::round(pos * k) & (k - 1);
            assert(0 <= sample_pos && sample_pos < wave_data_.size());
            return wave_data_[sample_pos];
        }
        
        std::vector<AudioSample> wave_data_;
        int shift_;
    };
    
    using WaveTablePtr = std::shared_ptr<WaveTable>;
    using OscillatorType = App::TestWaveformType;
    
    //! This function calculates the PolyBLEPs
    //! @param t 現在の位相。[0..1]で、[0..2*PI]を表す
    //! @param dt 目的のピッチに対応する角速度を正規化周波数で表したもの。
    static
    double poly_blep(double t, double dt)
    {
        // t-t^2/2 +1/2
        // 0 < t <= 1
        // discontinuities between 0 & 1
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        
        // t^2/2 +t +1/2
        // -1 <= t <= 0
        // discontinuities between -1 & 0
        else if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        
        // no discontinuities
        // 0 otherwise
        else return 0.0;
    }
    
    static
    double note_number_to_freq(int n) {
        return 440 * pow(2, (n - 69) / 12.0);
    };
    
    static
    WaveTablePtr GenerateSinTable()
    {
        auto table = std::make_unique<WaveTable>(16);
        auto data = table->wave_data_.data();
        auto const len = table->wave_data_.size();
        for(int smp = 0; smp < len; ++smp) {
            data[smp] = sin(2 * M_PI * smp / (double)len);
        }

        return table;
    }
    
    static
    WaveTable const * GetSinTable()
    {
        static WaveTablePtr table = GenerateSinTable();
        return table.get();
    }
    
    struct Voice
    {
        Voice() = default;
        
        Voice(OscillatorType ot,
              double sample_rate,
              double freq,
              Int32 num_attack_samples,
              Int32 num_release_samples)
        {
            num_attack_samples_ = num_attack_samples;
            num_release_samples_ = num_release_samples;
            ot_ = ot;
            freq_ = freq;
            sample_rate_ = sample_rate;
        }
        
        double get_current_gain() const
        {
            double const kVolumeRange = 48.0;
            
            double gain = 1.0;
            if(state_ == State::kAttack) {
                assert(attack_sample_pos_ < num_attack_samples_);
                if(attack_sample_pos_ == 0) {
                    return 0; // gain == 0
                } else {
                    gain = DBToLinear((attack_sample_pos_ / (double)num_attack_samples_) * kVolumeRange - kVolumeRange);
                }
            } else if(state_ == State::kSustain) {
                // do nothing.
            } else if(state_ == State::kRelease) {
                assert(release_sample_pos_ < num_release_samples_);
                auto attack_pos_ratio = attack_sample_pos_ / (double)num_attack_samples_;
                auto release_pos_ratio = Clamp<double>((release_sample_pos_ / (double)num_release_samples_) + (1 - attack_pos_ratio),
                                                       -1.0, 1.0);
                gain = DBToLinear(release_pos_ratio * -kVolumeRange);
            }
            
            return gain;
        }
        
        //! generate one sample and advance one step.
        AudioSample generate()
        {
            if(is_alive() == false) { return 0; }
            
            auto const gain = get_current_gain();
            
            auto const dt = freq_ / sample_rate_;
            auto const t = t_;
            
            double smp = 0;
            
            // based on http://www.martin-finke.de/blog/articles/audio-plugins-018-polyblep-oscillator/
            if(ot_ == OscillatorType::kSine) {
                auto const wave = GetSinTable();
                assert(wave);
                smp = wave->GetValueSimple(t);
            } else if(ot_ == OscillatorType::kSaw) {
                smp = ((2 * t) - 1.0) - poly_blep(t, dt);
            } else if(ot_ == OscillatorType::kSquare) {
                smp = t < 0.5 ? 1.0 : -1.0;
                smp += poly_blep(t, dt);
                smp -= poly_blep(fmod(t + 0.5, 1.0), dt);
            } else if(ot_ == OscillatorType::kTriangle) {
                smp = t < 0.5 ? 1.0 : -1.0;
                smp += poly_blep(t, dt);
                smp -= poly_blep(fmod(t + 0.5, 1.0), dt);
                
                auto dt2pi = dt * 2 * M_PI;
                smp = dt2pi * smp + (1 - dt2pi) * last_smp_;
                last_smp_ = smp;
            }
            
            // advance one step
            t_ += dt;
            while(t_ >= 1.0) {
                t_ -= 1.0;
            }
            
            advance_envelope();
            
            double const master_volume = 0.5;
            return smp * gain * master_volume;
        }
        
        bool is_alive() const { return state_ != State::kFinished; }
        
        void start()
        {
            t_ = 0;
            last_smp_ = 0;
            attack_sample_pos_ = 0;
            release_sample_pos_ = 0;
            state_ = State::kAttack;
        }
        
        void force_stop()
        {
            state_ = State::kFinished;
        }
        
        void request_to_stop()
        {
            if(state_ != State::kFinished) {
                state_ = State::kRelease;
            }
        }

    private:
        Int32 num_attack_samples_ = 0;
        Int32 attack_sample_pos_ = 0;
        Int32 num_release_samples_ = 0;
        Int32 release_sample_pos_ = 0;
        enum class State {
            kAttack,
            kSustain,
            kRelease,
            kFinished,
        };
        State state_ = State::kFinished;
        
        OscillatorType ot_;
        double freq_;
        double sample_rate_;
        double t_ = 0;
        double last_smp_ = 0;
        
        void advance_envelope()
        {
            if(state_ == State::kAttack) {
                attack_sample_pos_ += 1;
                if(attack_sample_pos_ == num_attack_samples_) {
                    state_ = State::kSustain;
                }
            } else if(state_ == State::kSustain) {
                // do nothing.
            } else if(state_ == State::kRelease) {
                release_sample_pos_ += 1;
                if(release_sample_pos_ == num_release_samples_) {
                    state_ = State::kFinished;
                }
            }
        }
    };
    
    std::array<Voice, 128> voices_;
    using KeyboardStatus = std::array<std::atomic<NoteStatus>, 128>;
    KeyboardStatus keys_;
    double sample_rate_ = 0;
    std::atomic<OscillatorType> ot_;
    Int32 num_attack_samples_ = 0;
    Int32 num_release_samples_ = 0;
    
    // デバイスダイアログを閉じて、デバイスを開始する前に段階で呼び出す
    void SetSampleRate(double sample_rate)
    {
        sample_rate_ = sample_rate;
        num_attack_samples_ = std::round(sample_rate * 0.03);
        num_release_samples_ = std::round(sample_rate * 0.03);
        
        for(auto &v: voices_) { v.force_stop(); }
        
        GetSinTable(); // force generate the sin table.
    }
    
    void SetOscillatorType(OscillatorType ot)
    {
        ot_.store(ot);
    }
    
    OscillatorType GetOscillatorType() const
    {
        return ot_.load();
    }
    
    void Process(AudioSample *dest, SampleCount length)
    {
        for(int n = 0; n < 128; ++n) {
            auto st = keys_[n].load();
            if(st.is_note_on() && voices_[n].is_alive() == false) {
                voices_[n] = Voice {
                    ot_, sample_rate_, note_number_to_freq(n),
                    num_attack_samples_, num_release_samples_
                };
                voices_[n].start();
            } else if(st.is_note_off()) {
                voices_[n].request_to_stop();
            }
            
            if(voices_[n].is_alive() == false) { continue; }
            
            auto &v = voices_[n];
            for(int smp = 0; smp < length; ++smp) {
                dest[smp] += v.generate();
            }
        }
    }
};

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
    TestSynth test_synth_;
    
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
        
        int const num_po = plugin_->GetNumOutputs();
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
        
        bool const use_dummy_synth = (!plugin_ || plugin_->GetComponentInfo().is_fx());

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
}

void App::SetTestWaveformType(TestWaveformType wt)
{
    pimpl_->test_synth_.SetOscillatorType(wt);
}

App::TestWaveformType App::GetTestWaveformType() const
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

void App::ShowAboutDialog()
{
    auto dlg = CreateAboutDialog();
 
    if(dlg->IsOk() == false) {
        wxMessageBox("Aboutダイアログの作成に失敗しました。", "Error", wxOK|wxCENTER|wxICON_ERROR);
        return;
    }
    
    dlg->ShowModal();
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
