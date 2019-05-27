#include "DeviceSettingDialog.hpp"
#include "Util.hpp"
#include "../device/AudioDeviceManager.hpp"

NS_HWM_BEGIN

BrushPen const kPanelBackgroundColour = wxColour(0x09, 0x21, 0x33);

class DeviceSettingPanel
:   public wxPanel
{
    struct AudioDeviceInfoWrapper : wxClientData
    {
        AudioDeviceInfoWrapper(AudioDeviceInfo const &info)
        :   info_(info)
        {}
        
        AudioDeviceInfo info_;
    };
    
    struct DeviceSetting
    {
        DeviceSetting() {}
        DeviceSetting(AudioDevice *dev)
        {
            auto get_info = [](auto dev, auto io_type) -> std::optional<AudioDeviceInfo> {
                auto p = dev->GetDeviceInfo(io_type);
                if(p) { return *p; }
                else { return std::nullopt; }
            };
            
            input_info_ = get_info(dev, DeviceIOType::kInput);
            output_info_ = get_info(dev, DeviceIOType::kOutput);
            sample_rate_ = dev->GetSampleRate();
            block_size_ = dev->GetBlockSize();
        }
        
        std::vector<double> GetAvailableSamplingRates() const
        {
            if(input_info_ && output_info_) {
                std::vector<double> const &xs = output_info_->supported_sample_rates_;
                std::vector<double> dest;
                std::copy_if(xs.begin(), xs.end(), std::back_inserter(dest), [this](auto x) {
                    return input_info_->IsSampleRateSupported(x);
                });
                return dest;
            } else if(input_info_) {
                return input_info_->supported_sample_rates_;
            } else if(output_info_) {
                return output_info_->supported_sample_rates_;
            } else {
                return {};
            }
        }
        
        std::optional<AudioDeviceInfo> input_info_;
        std::optional<AudioDeviceInfo> output_info_;
        double sample_rate_ = 0;
        SampleCount block_size_ = 0;
    };
    
    //! 最新のデバイス状態を表す
    DeviceSetting device_setting_;
    
public:
    DeviceSettingPanel(wxWindow *parent)
    :   wxPanel(parent)
    {
        st_audio_inputs_ = new wxStaticText(this, wxID_ANY, "Audio Input: ");
        cho_audio_inputs_ = new wxChoice(this, wxID_ANY);
        st_audio_outputs_ = new wxStaticText(this, wxID_ANY, "Audio Output: ");
        cho_audio_outputs_ = new wxChoice(this, wxID_ANY);
        st_sample_rates_ = new wxStaticText(this, wxID_ANY, "Sample Rate: ");
        cho_sample_rates_ = new wxChoice(this, wxID_ANY);
        st_buffer_sizes_ = new wxStaticText(this, wxID_ANY, "Buffer Size: ");
        cho_buffer_sizes_ = new wxChoice(this, wxID_ANY);
        
        st_audio_inputs_->SetForegroundColour(HSVToColour(0.0, 0.0, 0.9));
        st_audio_inputs_->SetBackgroundColour(kPanelBackgroundColour.brush_.GetColour());
        
        st_audio_outputs_->SetForegroundColour(HSVToColour(0.0, 0.0, 0.9));
        st_audio_outputs_->SetBackgroundColour(kPanelBackgroundColour.brush_.GetColour());
        
        st_sample_rates_->SetForegroundColour(HSVToColour(0.0, 0.0, 0.9));
        st_sample_rates_->SetBackgroundColour(kPanelBackgroundColour.brush_.GetColour());
        
        st_buffer_sizes_->SetForegroundColour(HSVToColour(0.0, 0.0, 0.9));
        st_buffer_sizes_->SetBackgroundColour(kPanelBackgroundColour.brush_.GetColour());
        
        auto vbox = new wxBoxSizer(wxVERTICAL);
        
        auto add_entry = [&](auto parent_box, auto static_text, auto choice) {
            auto hbox = new wxBoxSizer(wxHORIZONTAL);
            static_text->SetMinSize(wxSize(150, 1));
            hbox->Add(static_text, wxSizerFlags(0).Expand());
            hbox->Add(choice, wxSizerFlags(1).Expand());
            parent_box->Add(hbox, wxSizerFlags(0).Expand().Border(wxTOP|wxBOTTOM, 5));
        };
        
        add_entry(vbox, st_audio_inputs_, cho_audio_inputs_);
        add_entry(vbox, st_audio_outputs_, cho_audio_outputs_);
        add_entry(vbox, st_sample_rates_, cho_sample_rates_);
        add_entry(vbox, st_buffer_sizes_, cho_buffer_sizes_);
        
        auto outer_box = new wxBoxSizer(wxHORIZONTAL);
        outer_box->Add(vbox, wxSizerFlags(1).Expand().Border(wxALL,  5));
        SetSizer(outer_box);
        
        Bind(wxEVT_PAINT, [this](auto &ev) { OnPaint(); });
        
        cho_audio_inputs_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectAudioInput(); });
        cho_audio_outputs_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectAudioOutput(); });
        cho_sample_rates_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectSampleRate(); });
        cho_buffer_sizes_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectBufferSize(); });
        
        SetAutoLayout(true);
        SetCanFocus(false);
        InitializeList();
        Layout();
    }
    
    bool IsOutputDeviceAvailable() const
    {
        return cho_audio_outputs_->GetCount();
    }
    
    //! 2つのAudioDeviceInfoが指すデバイスが同じものかどうかを返す。
    //! サポートしているサンプリングレートの違いは関知しない。
    static
    bool is_same_device(AudioDeviceInfo const &x, AudioDeviceInfo const &y)
    {
        auto to_tuple = [](auto const &info) {
            return std::tie(info.driver_, info.io_type_, info.name_, info.num_channels_);
        };
        
        return to_tuple(x) == to_tuple(y);
    }
    
    template<class T>
    static
    T * to_ptr(std::optional<T> &opt) { return (opt ? &*opt : nullptr); }
    
    template<class T>
    static
    T const * to_ptr(std::optional<T> const &opt) { return (opt ? &*opt : nullptr); }
    
    template<class T>
    static
    std::optional<T> to_optional(T *p) { return (p ? *p : std::optional<T>{}); }
    
    void OnSelectAudioInput()
    {
        auto *cho = cho_audio_inputs_;
        
        auto sel = cho->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        auto copied = device_setting_;
        auto wrapper = dynamic_cast<AudioDeviceInfoWrapper *>(cho->GetClientObject(sel));
        
        if(is_same_device(copied.input_info_.value_or(AudioDeviceInfo{}),
                          wrapper ? wrapper->info_ : AudioDeviceInfo{}))
        {
            return;
        }
        
        copied.input_info_ = wrapper ? wrapper->info_ : std::optional<AudioDeviceInfo>{};
        OpenDevice(copied, true);
    }
    
    void OnSelectAudioOutput()
    {
        auto *cho = cho_audio_outputs_;
        
        auto sel = cho->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        auto copied = device_setting_;
        auto wrapper = dynamic_cast<AudioDeviceInfoWrapper *>(cho->GetClientObject(sel));
        assert(wrapper);
        
        if(is_same_device(copied.output_info_.value_or(AudioDeviceInfo{}),
                          wrapper->info_))
        {
            return;
        }
        
        copied.output_info_ = wrapper->info_;
        OpenDevice(copied, false);
    }
    
    void OnSelectSampleRate()
    {
        auto *cho = cho_sample_rates_;
        
        auto sel = cho->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        auto copied = device_setting_;
        double new_rate = 0;
        if(cho->GetStringSelection().ToDouble(&new_rate) == false) { return; }
        
        if(copied.sample_rate_ == new_rate) { return; }
        
        copied.sample_rate_ = new_rate;
        OpenDevice(copied, false);
    }
    
    void OnSelectBufferSize()
    {
        auto *cho = cho_buffer_sizes_;
        
        auto sel = cho->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        auto copied = device_setting_;
        long new_block_size = 0;
        if(cho->GetStringSelection().ToLong(&new_block_size) == false) { return; }
        
        if(copied.block_size_ == new_block_size) { return; }
        
        copied.block_size_ = new_block_size;
        OpenDevice(copied, false);
    }
    
    std::optional<AudioDeviceInfo> GetOppositeDeviceInfo(AudioDeviceInfo const &info) const
    {
        wxChoice *op_cho;
        int index_start = 0;
        
        if(info.io_type_ == DeviceIOType::kInput) {
            op_cho = cho_audio_outputs_;
            index_start = 0;
        } else {
            op_cho = cho_audio_inputs_;
            index_start = 1;
        }
        
        // 同じDriverのものだけ集める
        std::vector<AudioDeviceInfo const *> xs;
        for(int i = index_start; i < op_cho->GetCount(); ++i) {
            auto p = dynamic_cast<AudioDeviceInfoWrapper *>(op_cho->GetClientObject(i));
            assert(p);
            
            if(p->info_.driver_ == info.driver_) {
                xs.push_back(&p->info_);
            }
        }
        if(xs.empty()) { return std::nullopt; }
        
        auto found = std::find_if(xs.begin(), xs.end(), [&info](auto x) {
            return x->name_ == info.name_;
        });
        
        if(found != xs.end()) { return **found; }
        return *xs[0];
    }
    
    void OpenDevice(DeviceSetting new_setting, bool is_input_changed)
    {
        //! an output device must be selected.
        if(!new_setting.output_info_) {
            return;
        }
        
        if(new_setting.block_size_ == 0) {
            new_setting.block_size_ = kSupportedBlockSizeDefault;
        }
        
        if(new_setting.sample_rate_ == 0) {
            new_setting.sample_rate_ = kSupportedSampleRateDefault;
        }
        
        auto adm = AudioDeviceManager::GetInstance();
        adm->Close();
        
        auto open_impl = [](DeviceSetting &ds) -> AudioDeviceManager::OpenResult {
            auto in = to_ptr(ds.input_info_);
            auto out = to_ptr(ds.output_info_);
            assert(out);
            
            auto rate = ds.sample_rate_;
            auto block = ds.block_size_;
            
            auto available_sample_rates = ds.GetAvailableSamplingRates();
            if(available_sample_rates.empty()) {
                //! 適用可能なサンプリングレートがない
                return AudioDeviceManager::Error(AudioDeviceManager::kInvalidParameters,
                                                 L"No valid sampling rates.");
            }
            
            if(std::none_of(available_sample_rates.begin(),
                            available_sample_rates.end(),
                            [rate](auto x) { return x == rate; }))
            {
                rate = available_sample_rates[0];
            }
            
            auto adm = AudioDeviceManager::GetInstance();
            auto result = adm->Open(in, out, rate, block);
            if(result.is_right()) {
                ds.sample_rate_ = rate;
            }
            
            return result;
        };
        
        auto result = open_impl(new_setting);
        
        if(result.is_right() == false) {
            if(is_input_changed && new_setting.input_info_) {
                auto info = GetOppositeDeviceInfo(*new_setting.input_info_);
                if(info) {
                    new_setting.output_info_ = info;
                    result = open_impl(new_setting);
                }
            } else {
                auto info = GetOppositeDeviceInfo(*new_setting.output_info_);
                new_setting.input_info_ = info;
                result = open_impl(new_setting);
            }
        }
        
        if(result.is_right() == false) {
            auto p = dynamic_cast<AudioDeviceInfoWrapper *>(cho_audio_outputs_->GetClientObject(0));
            assert(p);
            new_setting.input_info_ = std::nullopt;
            new_setting.output_info_ = p->info_;
            result = open_impl(new_setting);
        }
        
        if(result.is_right()) {
            device_setting_ = new_setting;
            UpdateSelections();
        } else {
            wxMessageBox(L"Opening audio device failed: " + result.left().error_msg_);
        }
    }
    
    //! デバイス名とドライバ名のセット
    static
    String get_device_label(AudioDeviceInfo const &info)
    {
        return info.name_ + L" (" + to_wstring(info.driver_) + L")";
    }
    
    static
    String sample_rate_to_string(double rate)
    {
        std::wstringstream ss;
        ss << rate;
        return ss.str();
    }
    
    void UpdateSelections()
    {
        auto select = [](wxChoice *cho, String const &label, int default_index) {
            for(int i = 0; i < cho->GetCount(); ++i) {
                if(cho->GetString(i) == label) {
                    cho->SetSelection(i);
                    return;
                }
            }
            
            cho->SetSelection(default_index);
        };
        
        if(device_setting_.input_info_) {
            select(cho_audio_inputs_, get_device_label(*device_setting_.input_info_), 0);
        } else {
            cho_audio_inputs_->SetSelection(0);
        }
        
        assert(device_setting_.output_info_);
        select(cho_audio_outputs_, get_device_label(*device_setting_.output_info_), wxNOT_FOUND);
        
        cho_sample_rates_->Clear();
        auto available = device_setting_.GetAvailableSamplingRates();
        for(auto rate: available) { cho_sample_rates_->Append(sample_rate_to_string(rate)); }
        select(cho_sample_rates_, sample_rate_to_string(device_setting_.sample_rate_), 0);
        
        cho_buffer_sizes_->Clear();
        
        auto const block_size_list = { 16, 32, 64, 128, 256, 378, 512, 768, 1024, 2048, 4096, 8192 };
        assert(*block_size_list.begin() == kSupportedBlockSizeMin);
        assert(*(block_size_list.end()-1) == kSupportedBlockSizeMax);
        
        for(auto block: block_size_list) {
            cho_buffer_sizes_->Append(std::to_string(block));
        }
        select(cho_buffer_sizes_, std::to_wstring(device_setting_.block_size_), 0);
    }
    
    void InitializeList()
    {
        auto adm = AudioDeviceManager::GetInstance();
        if(adm->IsOpened()) {
            device_setting_ = DeviceSetting(adm->GetDevice());
        }
        adm->Close();
        
        auto list = adm->Enumerate();
        
        cho_audio_inputs_->Clear();
        cho_audio_outputs_->Clear();
        cho_sample_rates_->Clear();
        cho_buffer_sizes_->Clear();
        
        cho_audio_inputs_->Append("Disable");
        
        int input_index = 0;
        int output_index = wxNOT_FOUND;
        
        for(auto &entry: list) {
            if(entry.io_type_ == DeviceIOType::kInput) {
                cho_audio_inputs_->Append(get_device_label(entry), new AudioDeviceInfoWrapper{entry});
                if(device_setting_.input_info_ && is_same_device(*device_setting_.input_info_, entry)) {
                    input_index = cho_audio_inputs_->GetCount() - 1;
                }
            } else {
                cho_audio_outputs_->Append(get_device_label(entry), new AudioDeviceInfoWrapper{entry});
                if(device_setting_.output_info_ && is_same_device(*device_setting_.output_info_, entry)) {
                    output_index = cho_audio_outputs_->GetCount() - 1;
                }
            }
        }
        
        if(cho_audio_outputs_->GetCount() == 0) {
            hwm::dout << "[Error] there's no audio output devices." << std::endl;
            return;
        }
        
        //! 出力デバイスは少なくとも１つ選ばれている状態にする
        if(!device_setting_.output_info_) {
            auto p = dynamic_cast<AudioDeviceInfoWrapper *>(cho_audio_outputs_->GetClientObject(0));
            device_setting_.output_info_ = p->info_;
            output_index = 0;
        }
        
        cho_audio_inputs_->SetSelection(input_index);
        cho_audio_outputs_->SetSelection(output_index);
        
        OpenDevice(device_setting_, false);
    }
    
    void OnPaint()
    {
        wxPaintDC pdc(this);
        wxGCDC dc(pdc);
        
        kPanelBackgroundColour.ApplyTo(dc);
        dc.DrawRectangle(GetClientRect());
    }
    
private:
    wxStaticText *st_audio_inputs_ = nullptr;
    wxChoice *cho_audio_inputs_ = nullptr;
    wxStaticText *st_audio_outputs_ = nullptr;
    wxChoice *cho_audio_outputs_ = nullptr;
    wxStaticText *st_sample_rates_ = nullptr;
    wxChoice *cho_sample_rates_ = nullptr;
    wxStaticText *st_buffer_sizes_ = nullptr;
    wxChoice *cho_buffer_sizes_ = nullptr;
};

class DeviceSettingDialog
:   public wxDialog
{
    wxSize const kMinSize = { 400, 200 };
    wxSize const kMaxSize = { 1000, 200 };

public:
    DeviceSettingDialog(wxWindow *parent)
    :   wxDialog(parent, wxID_ANY, "Device Setting", 
                 wxDefaultPosition, kMinSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        SetMinClientSize(kMinSize);
        SetMaxClientSize(kMaxSize);
        SetClientSize(kMinSize);
        
        panel_ = new DeviceSettingPanel(this);
        panel_->Show();
        
        auto sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(panel_, wxSizerFlags(1).Expand());
        
        SetSizer(sizer);
    }
    
    int ShowModal() override
    {
        if(panel_->IsOutputDeviceAvailable() == false) {
            return wxID_CANCEL;
        }
        
        return wxDialog::ShowModal();
    }
    
    DeviceSettingPanel *panel_;
};

wxDialog * CreateDeviceSettingDialog(wxWindow *parent)
{
    return new DeviceSettingDialog(parent);
}

NS_HWM_END
