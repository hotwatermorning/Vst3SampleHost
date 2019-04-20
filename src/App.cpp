#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Button.H>
#include <device/AudioDeviceManager.hpp>
#include <plugin/vst3/Vst3Plugin.hpp>
#include <plugin/vst3/Vst3PluginFactory.hpp>
#include <plugin/PluginScanner.hpp>
#include <misc/StrCnv.hpp>

using namespace hwm;

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

struct AudioCallback : IAudioDeviceCallback
{
public:
    AudioCallback(Vst3Plugin *plugin)
    :   plugin_(plugin)
    {}
    
    void StartProcessing(double sample_rate,
                         SampleCount max_block_size,
                         int num_input_channels,
                         int num_output_channels) override
    {
        num_input_channels_ = num_input_channels;
        num_output_channels_ = num_output_channels;
        continuous_sample_count_ = 0;
        sample_rate_ = sample_rate;
    }
    
    void Process(SampleCount block_size,
                 float const * const * input,
                 float **output) override
    {
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
        
        plugin_->Process(pi);
    }
    
    void StopProcessing() override
    {}
    
private:
    int num_input_channels_ = 0;
    int num_output_channels_ = 0;
    int continuous_sample_count_ = 0;
    double sample_rate_ = 0;
    Vst3Plugin *plugin_;
};

std::wstring ChooseVst3Module()
{
    std::wstring module_path;
    
    for( ; ; ) {
        Fl_Native_File_Chooser chooser;
        chooser.title("Pick a VST3 Module File");
        chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
        chooser.filter("Vst3 Module\t*.vst3\n");
#if defined(_MSC_VER)
        chooser.directory("C:/Program Files/Common Files/VST3");
#else
        chooser.directory("/Library/Audio/Plug-Ins/VST3");
#endif
        
        // Show native chooser
        auto const result = chooser.show();
        
        if(result == -1) {
            fl_alert("ERROR: %s\n", chooser.errmsg());
            continue;
        }
        
        if(result == 1) {
            printf("CANCELED\n");
            return {};
        }
        
        return to_wstr(chooser.filename());
    }
}

struct App
:   SingleInstance<App>
{
    App()
    {
        factory_list_ = std::make_shared<Vst3PluginFactoryList>();
    }
    
    AudioDeviceManager adm;
    
    bool OpenVst3Module(std::wstring path)
    {
        auto factory = factory_list_->FindOrCreateFactory(path);
        if(!factory) {
            hwm::dout << "Failed to open module.";
            return false;
        }
        
        factory_ = std::move(factory);
        return true;
    }
    
    bool OpenVst3Plugin(std::string cid);
    
    Vst3Plugin * GetPlugin()
    {
        return plugin_.get();
    }
    
    std::shared_ptr<Vst3PluginFactoryList> factory_list_;
    std::shared_ptr<Vst3PluginFactory> factory_;
    std::shared_ptr<Vst3Plugin> plugin_;
};

struct MainWindow : public Fl_Double_Window
{
    static constexpr int kMinWidth = 600;
    static constexpr int kMinHeight = 400;
    static constexpr int kMaxWidth = 1000;
    static constexpr int kMaxHeight = 900;
    static constexpr int kLoadButtonWidth = 100;
    static constexpr int kComponentHeight = 50;
    using base_type = Fl_Double_Window;
    using this_type = MainWindow;
    
    MainWindow(int x, int y, int w, int h)
    :   Fl_Double_Window(x, y, std::max<int>(kMinWidth, w), std::max<int>(kMinHeight, h))
    ,   module_path_box_(0, 0, 1, 1)
    ,   load_btn_(0, 0, 1, 1, "Open")
    ,   select_component_(0, 0, 1, 1, "")
    {
        module_path_box_.labelfont(252);
        load_btn_.callback(&MainWindow::OnOpenVst3Module, this);
        size_range(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
        Layout();
    }
    
    void Layout()
    {
        module_path_box_.resize(20,
                                20,
                                w() - 60 - kLoadButtonWidth,
                                kComponentHeight);
        
        load_btn_.resize(w() - 20 - kLoadButtonWidth,
                         20,
                         kLoadButtonWidth,
                         kComponentHeight);
        
        select_component_.resize(20,
                                 20 * 2 + kComponentHeight,
                                 w() - 20 * 2,
                                 kComponentHeight);
    }
    
    virtual void resize(int x, int y, int w, int h) override
    {
        w = std::max<int>(w, kMinWidth);
        h = std::max<int>(h, kMinHeight);
        base_type::resize(x, y, w, h);
        Layout();
    }
    
    void OnLoadVst3ModuleImpl(Fl_Widget *w)
    {
        std::wstring path = ChooseVst3Module();
        App *app = App::GetInstance();
        bool opened = app->OpenVst3Module(path);
        if(!opened) { return; }
        
        module_path_box_.label(to_utf8(path).c_str());
    }
    
    static
    void OnOpenVst3Module(Fl_Widget *w, void *p) { ((this_type *)p)->OnLoadVst3ModuleImpl(w); }
    
    void OnSelectPluginImpl(Fl_Widget *w)
    {
        
    }

    static
    void OnSelectPlugin(Fl_Widget *w, void *p) { ((this_type *)p)->OnSelectPluginImpl(w); }
    
    Fl_Box module_path_box_;
    Fl_Button load_btn_;
    Fl_Choice select_component_;
    Fl_End end_;
};

int main(int argc, char **argv) {
    Fl_Font num = Fl::set_fonts();
    std::cout << "font num: " << num << std::endl;
    
    for(int i = 0; i < num; ++i) {
        std::cout << i << " : " << Fl::get_font_name(i) << std::endl;
    }
    App app;
    
    auto adm = AudioDeviceManager::GetInstance();
    auto list = adm->Enumerate();
    for(auto &device_info: list) {
        if(device_info.io_type_ == DeviceIOType::kOutput) {
            adm->Open(nullptr, &device_info, kSampleRate, kBlockSize);
            break;
        }
    }
    
    auto wnd = new MainWindow(0, 0, 400,180);
    
    wnd->end();
    wnd->show(argc, argv);
    
    auto const result = Fl::run();
    
    if(auto *dev = adm->GetDevice()) {
        dev->Stop();
        adm->Close();
    }
    
    return result;
}

