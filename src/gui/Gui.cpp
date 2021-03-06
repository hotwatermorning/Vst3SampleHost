#include "Gui.hpp"
#include "../app/App.hpp"

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/dir.h>

#include "../resource/ResourceHelper.hpp"
#include "../plugin/vst3/Vst3Plugin.hpp"
#include "../plugin/vst3/Vst3PluginFactory.hpp"
#include "../device/AudioDeviceManager.hpp"
#include "../misc/MathUtil.hpp"
#include "./Util.hpp"
#include "./Keyboard.hpp"
#include "./PluginEditor.hpp"
#include "./PCKeyboardInput.hpp"

NS_HWM_BEGIN

class LevelMeterPanel
:   public wxPanel
{
public:
    LevelMeterPanel(wxWindow *parent)
    :   wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    {
        SetBackgroundColour(*wxBLACK);

        font_ = wxFontInfo(wxSize{10, 10}).Family(wxFONTFAMILY_MODERN);
        Bind(wxEVT_PAINT, [this](auto &) { OnPaint(); });
        Bind(wxEVT_SIZE, [this](auto &) {
            auto size = GetClientSize();
            if(size.x <= 0 || size.y <= 0) { return; }
            bmp_ = GraphicsBuffer(size);
        });

        timer_.Bind(wxEVT_TIMER, [this](auto &) { Refresh(); });
        
        timer_.Start(10);
    }
    
    void OnPaint()
    {
        if(bmp_.IsOk() == false) { return; }

        wxMemoryDC memory_dc(bmp_.GetBitmap());
        wxGCDC dc(memory_dc);
        
        dc.SetPen(wxPen(HSVToColour(0, 0, 0.0)));
        dc.SetBrush(wxBrush(HSVToColour(0, 0, 0.0)));
        dc.DrawRectangle(GetClientRect());
        
        auto app = App::GetInstance();
        auto adm = AudioDeviceManager::GetInstance();
        assert(adm);
        auto dev = adm->GetDevice();
        if(!dev) {
            BrushPen bp { HSVToColour(0.0, 0.0, 0.4) };
            bp.ApplyTo(dc);
            dc.DrawRectangle(GetClientRect());
            
            dc.SetFont(font_);
            dc.SetTextForeground(HSVToColour(0.0, 0.0, 0.85));
            dc.DrawLabel("No Device", GetClientRect(), wxALIGN_CENTER);
            
            wxPaintDC pdc(this);
            pdc.Blit(wxPoint{}, GetClientSize(), &memory_dc, wxPoint{});
            return;
        }
        
        auto const num_ch = dev->GetDeviceInfo(DeviceIOType::kOutput)->num_channels_;
        if(level_meter_.size() != num_ch) {
            level_meter_.resize(num_ch);
        }
        
        double const kViewMinDB = app->GetAudioOutputMinLevel();
        double const kViewMaxDB = 10.0;
        
        app->GetAudioOutputLevelMeter(level_meter_);
        auto const size = GetClientSize();
        
        int const bars_ysum = size.y - (num_ch - 1);
        int const bar_height = bars_ysum / num_ch;
        int const bars_height = bar_height * num_ch + (num_ch - 1);
        if(bar_height == 0) {
            BrushPen bp { HSVToColour(0.0, 0.0, 0.4) };
            bp.ApplyTo(dc);
            dc.DrawRectangle(GetClientRect());
            
            dc.SetFont(font_);
            dc.SetTextForeground(HSVToColour(0.0, 0.0, 0.85));
            dc.DrawLabel("Too Many Channels", GetClientRect(), wxALIGN_CENTER);
            
            wxPaintDC pdc(this);
            pdc.Blit(wxPoint{}, GetClientSize(), &memory_dc, wxPoint{});
            return;
        }
        
        {
            auto rc = GetClientRect();
            rc.SetHeight(bars_height);
            dc.GradientFillLinear(rc, HSVToColour(0.2, 1.0, 1.0), HSVToColour(0.0, 1.0, 1.0));
        }
        
        for(int ch = 0; ch < num_ch; ++ch) {
            auto top = (bar_height + 1) * ch;
            
            if(ch != num_ch - 1) {
                dc.SetPen(wxPen(HSVToColour(0, 0, 0)));
                auto gap_y = top + bar_height;
                dc.DrawLine(0, gap_y, size.x, gap_y);
            }
            
            auto const cur = Clamp<double>(level_meter_[ch], kViewMinDB, kViewMaxDB);
            int const left_pos = std::round(size.x * (cur - kViewMinDB) / (kViewMaxDB - kViewMinDB));
            wxRect rc {
                wxPoint { left_pos, top },
                wxSize { size.x - left_pos, bar_height }
            };
            dc.SetPen(wxPen(HSVToColour(0, 0, 0.2)));
            dc.SetBrush(wxBrush(HSVToColour(0, 0, 0.2)));
            dc.DrawRectangle(rc);
        }
        
        auto const kZeroDB = 0;
        dc.SetPen(HSVToColour(0.4, 0.6, 1.0, 0.34));
        int const left_pos = std::round(size.x * (kZeroDB - kViewMinDB) / (kViewMaxDB - kViewMinDB));
        dc.DrawLine(left_pos, 0, left_pos, bars_height-1);

        wxPaintDC pdc(this);
        pdc.Blit(wxPoint{}, GetClientSize(), &memory_dc, wxPoint{});
    }
    
private:
    bool AcceptsFocus() const override { return false; }
    
    wxFont font_;
    wxTimer timer_;
    std::vector<double> level_meter_;
    GraphicsBuffer bmp_;
};

class LevelSliderPanel
:   public wxPanel
,   public App::IPlaybackOptionChangeListener
{
    wxColour const kColLabel = HSVToColour(0.0, 0.0, 0.9);
    wxColour const kColBackgrund = HSVToColour(0.7, 0.4, 0.4);
    double const kVolumeSliderScale = 100.0;
    
public:
    LevelSliderPanel(wxWindow *parent)
    :   wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    {
        SetBackgroundColour(kColBackgrund);
        
        auto app = App::GetInstance();
        auto const min_level = app->GetAudioOutputMinLevel();
        auto const max_level = app->GetAudioOutputMaxLevel();
        auto const cur_level = app->GetAudioOutputLevel();
        
        lbl_volume_ = new wxStaticText(this, wxID_ANY, L"出力レベル");
        lbl_volume_->SetForegroundColour(kColLabel);
        lbl_volume_->SetMinSize(lbl_volume_->GetSize());
        //lbl_volume_->SetMinClientSize(lbl_volume_->GetTextExtent(lbl_volume_->GetLabel()));
        
        sl_volume_ = new wxSlider(this, wxID_ANY,
                                  cur_level * kVolumeSliderScale,
                                  min_level * kVolumeSliderScale,
                                  max_level * kVolumeSliderScale);
        sl_volume_->SetMinSize(wxSize{80, 1});
        
        lbl_current_level_ = new wxStaticText(this, wxID_ANY, L"", wxDefaultPosition, wxDefaultSize,
                                              wxST_NO_AUTORESIZE|wxALIGN_RIGHT);
        lbl_current_level_->SetForegroundColour(kColLabel);
        auto size = lbl_current_level_->GetTextExtent("-00.00 dB");
        size.IncBy(10, 0);
        lbl_current_level_->SetMinClientSize(size);
        
        auto vcenter_box = [](auto window) {
            auto vbox = new wxBoxSizer(wxVERTICAL);
            vbox->AddStretchSpacer(1);
            vbox->Add(window, wxSizerFlags(100).Expand());
            vbox->AddStretchSpacer(1);
            return vbox;
        };
        
        auto hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->Add(vcenter_box(lbl_volume_), wxSizerFlags(0).Expand());
        hbox->Add(sl_volume_, wxSizerFlags(1).Expand());
        hbox->Add(vcenter_box(lbl_current_level_), wxSizerFlags(0).Expand());
        
        SetSizer(hbox);
        
        sl_volume_->Bind(wxEVT_SLIDER, [this](wxCommandEvent &ev) { OnSlider(ev); });
        
        slr_pocl_.reset(app->GetPlaybackOptionChangeListenerService(), this);
        
        UpdateLevelText(cur_level);
    }
    
private:
    wxStaticText *lbl_volume_ = nullptr;
    wxSlider *sl_volume_ = nullptr;
    wxStaticText *lbl_current_level_ = nullptr;
    ScopedListenerRegister<App::IPlaybackOptionChangeListener> slr_pocl_;
    
    bool AcceptsFocus() const override { return false; }
    
    void OnSlider(wxCommandEvent &ev)
    {
        auto app = App::GetInstance();
        auto level = sl_volume_->GetValue() / kVolumeSliderScale;
        app->SetAudioOutputLevel(sl_volume_->GetValue() / kVolumeSliderScale);
        UpdateLevelText(level);
    }
    
    void UpdateLevelText(double level)
    {
        auto app = App::GetInstance();
        auto const min_level = app->GetAudioOutputMinLevel();
        
        char buf[32] = {};
        if(level <= min_level) {
            sprintf(buf, "-Inf dB");
        } else {
            sprintf(buf, "%.2f dB", level);
        }
        lbl_current_level_->SetLabel(buf);
    }
    
    void OnAudioOutputLevelChanged(double new_level) override
    {
        sl_volume_->SetValue(new_level * kVolumeSliderScale);
        UpdateLevelText(new_level);
    }
};

class AudioInputPanel
:   public wxPanel
,   public App::IPlaybackOptionChangeListener
{
    wxColour const kColLabel = HSVToColour(0.0, 0.0, 0.9);
    wxColour const kColInputCheckBox = HSVToColour(0.5, 0.4, 0.4);
    
public:
    AudioInputPanel(wxWindow *parent)
    :   wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    {
        SetBackgroundColour(kColInputCheckBox);
        
        auto app = App::GetInstance();
        
        btn_enable_input_ = new wxCheckBox(this, wxID_ANY, L"マイク入力",
                                           wxDefaultPosition);
        btn_enable_input_->SetForegroundColour(kColLabel);
        btn_enable_input_->SetBackgroundColour(kColInputCheckBox);
        btn_enable_input_->SetMinSize(wxSize(150, 1));
        
        btn_enable_input_->Enable(app->CanEnableAudioInput());
        btn_enable_input_->SetValue(app->IsAudioInputEnabled());
        
        btn_enable_input_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &ev) { OnCheckBox(ev); });
        
        auto vcenter_box = [](auto window) {
            auto vbox = new wxBoxSizer(wxVERTICAL);
            vbox->AddStretchSpacer(1);
            vbox->Add(window, wxSizerFlags(100).Expand());
            //vbox->SetMinSize(wxSize{window->GetClientSize().x, 1});
            vbox->AddStretchSpacer(1);
            return vbox;
        };
        
        auto hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->Add(vcenter_box(btn_enable_input_), wxSizerFlags(1).Expand());
        SetSizer(hbox);
        
        slr_pocl_.reset(app->GetPlaybackOptionChangeListenerService(), this);
    }
    
private:
    wxCheckBox *btn_enable_input_ = nullptr;
    ScopedListenerRegister<App::IPlaybackOptionChangeListener> slr_pocl_;
    
    bool AcceptsFocus() const override { return false; }
    
    void OnCheckBox(wxCommandEvent &ev)
    {
        auto app = App::GetInstance();
        app->EnableAudioInput(btn_enable_input_->IsChecked());
    }
    
    void OnAudioInputAvailabilityChanged(bool available) override
    {
        btn_enable_input_->Enable(available);
    }
    
    void OnAudioInputEnableStateChanged(bool enabled) override
    {
        btn_enable_input_->SetValue(enabled);
    }
};

class HeaderPanel
:   public wxPanel
{
public:
    HeaderPanel(wxWindow *parent)
    :   wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    {
        SetBackgroundColour(HSVToColour(0.0, 0.0, 0.1));
        
        level_slider_ = new LevelSliderPanel(this);
        level_meter_ = new LevelMeterPanel(this);
        level_meter_->SetMinSize(wxSize{70, 1});
        level_meter_->SetMaxSize(wxSize{300, 100});

        audio_input_ = new AudioInputPanel(this);
        audio_input_->SetMinSize(wxSize{100, 1});
        audio_input_->SetMaxSize(wxSize{200, 100});
        
        auto hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->Add(level_slider_, wxSizerFlags(5).Expand());
        hbox->AddSpacer(5);
        hbox->Add(level_meter_, wxSizerFlags(5).Expand());
        hbox->AddSpacer(5);
        hbox->Add(audio_input_, wxSizerFlags(1).Expand());
        
        auto vbox = new wxBoxSizer(wxVERTICAL);
        vbox->Add(hbox, wxSizerFlags(1).Expand().Border(wxALL, 5));
        SetSizer(vbox);
    }
    
    bool AcceptsFocus() const override { return false; }
    
private:
    LevelSliderPanel *level_slider_ = nullptr;
    LevelMeterPanel *level_meter_ = nullptr;
    AudioInputPanel *audio_input_ = nullptr;
};

class MainWindow
:   public wxWindow
,   public App::IModuleLoadListener
,   public App::IPluginLoadListener
,   public IPluginEditorFrameListener
{
public:
    static constexpr int kMinWidth = 600;
    static constexpr int kMinHeight = 400;
    static constexpr int kMaxWidth = 1000;
    static constexpr int kMaxHeight = 900;
    static constexpr int kLoadButtonWidth = 100;
    static constexpr int kComponentHeight = 50;
    
    MainWindow(wxWindow *parent, wxWindowID id = wxID_ANY,
               wxPoint pos = wxDefaultPosition,
               wxSize size = wxDefaultSize)
    :   wxWindow(parent, id, pos, size)
    {
        this->SetBackgroundColour(wxColour(0x09, 0x21, 0x33));
        
        header_panel_ = new HeaderPanel(this);
        header_panel_->SetMinClientSize(wxSize{1, 30});
        
        tc_filepath_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(100, 20), wxTE_READONLY);
        tc_filepath_->SetBackgroundColour(*wxWHITE);
        
        btn_load_module_ = new wxButton(this, wxID_ANY, "Load Module", wxDefaultPosition, wxSize(100, 20));
        
        st_factory_info_label_ = new wxStaticText(this, wxID_ANY, "Factory Info", wxDefaultPosition, wxSize(100, 20), wxST_NO_AUTORESIZE);
        st_factory_info_label_->SetForegroundColour(*wxWHITE);
        tc_factory_info_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(200, 20), wxTE_READONLY|wxTE_MULTILINE);
        tc_factory_info_->SetBackgroundColour(*wxWHITE);
        
        cho_select_component_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(100, 20));
        
        st_component_info_label_ = new wxStaticText(this, wxID_ANY, "Component Info", wxDefaultPosition, wxSize(100, 20), wxST_NO_AUTORESIZE);
        st_component_info_label_->SetForegroundColour(*wxWHITE);
        tc_component_info_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(100, 100), wxTE_READONLY|wxTE_MULTILINE);
        tc_component_info_->SetBackgroundColour(*wxWHITE);
        
        btn_open_editor_ = new wxButton(this, wxID_ANY, "Open Editor", wxDefaultPosition, wxSize(100, 20));
        
        keyboard_ = CreateKeyboardPanel(this);
        
        auto vbox = new wxBoxSizer(wxVERTICAL);
        vbox->Add(header_panel_, wxSizerFlags(0).Expand());
        
        auto hbox = new wxBoxSizer(wxHORIZONTAL);
        
        {
            hbox->AddStretchSpacer(1)->SetMinSize(wxSize(20, 0));
            
            auto vbox_inner = new wxBoxSizer(wxVERTICAL);
            
            vbox_inner->AddSpacer(20);
            
            auto browse_box = new wxBoxSizer(wxHORIZONTAL);
            
            browse_box->Add(tc_filepath_, wxSizerFlags(1).Expand().Border());
            browse_box->Add(btn_load_module_, wxSizerFlags().Expand().Border());
            
            vbox_inner->Add(browse_box, wxSizerFlags(0).Expand());
            
            vbox_factory_ = new wxBoxSizer(wxVERTICAL);
            vbox_factory_->Add(st_factory_info_label_, wxSizerFlags(0).Expand().Border(wxTOP|wxLEFT|wxRIGHT));
            vbox_factory_->Add(tc_factory_info_, wxSizerFlags(1).Expand().Border(wxBOTTOM|wxLEFT|wxRIGHT));
            vbox_factory_->Add(cho_select_component_, wxSizerFlags(0).Expand().Border());
            vbox_factory_->ShowItems(false);
            
            vbox_inner->Add(vbox_factory_, wxSizerFlags(1).Expand());
            
            vbox_component_ = new wxBoxSizer(wxVERTICAL);
            vbox_component_->Add(st_component_info_label_, wxSizerFlags(0).Expand().Border(wxTOP|wxLEFT|wxRIGHT));
            vbox_component_->Add(tc_component_info_, wxSizerFlags(1).Expand().Border(wxBOTTOM|wxLEFT|wxRIGHT));
            vbox_component_->Add(btn_open_editor_, wxSizerFlags(0).Align(wxALIGN_RIGHT).Border());
            vbox_component_->ShowItems(false);
            
            vbox_inner->Add(vbox_component_, wxSizerFlags(1).Expand());
            dummy_component_ = vbox_inner->AddStretchSpacer(1);
            dummy_component_->SetBorder(wxSizerFlags::GetDefaultBorder());
            vbox_inner->AddSpacer(20);
            
            hbox->Add(vbox_inner, wxSizerFlags(8).Expand());
            hbox->AddStretchSpacer(1)->SetMinSize(wxSize(20, 0));
        }
        
        vbox->Add(hbox, wxSizerFlags(1).Expand());
        {
            auto hbox = new wxBoxSizer(wxHORIZONTAL);
            hbox->AddStretchSpacer(1);
            hbox->Add(keyboard_, wxSizerFlags(10000).Expand());
            hbox->AddStretchSpacer(1);
            vbox->Add(hbox, wxSizerFlags(0).Expand());
        }
        
        SetSizerAndFit(vbox);
        
        Bind(wxEVT_PAINT, [this](auto &ev) { OnPaint(ev); });
        btn_load_module_->Bind(wxEVT_BUTTON, [this](auto &ev) { OnLoadModule(); });
        cho_select_component_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectComponent(); });
        btn_open_editor_->Bind(wxEVT_BUTTON, [this](auto &ev) { OnOpenEditor(); });
        
        auto app = App::GetInstance();
        slr_mll_.reset(app->GetModuleLoadListenerService(), this);
        slr_pll_.reset(app->GetPluginLoadListenerService(), this);
    }
    
    bool AcceptsFocus() const override { return false; }
    
    bool Destroy() override
    {
        OnCloseEditor();
        return wxWindow::Destroy();
    }
    
private:
    
    void OnAfterModuleLoaded(String path, Vst3PluginFactory *factory) override
    {
        tc_filepath_->Clear();
        tc_filepath_->AppendText(path.c_str());
        
        vbox_factory_->ShowItems(true);
        auto fi = factory->GetFactoryInfo();
        
        String fi_str =
        wxString::Format(L"vendor: %ls\nurl: %ls\ne-mail: %ls\n"
                         L"discardable: %ls\nlicense_check: %ls\ncomponent non discardable: %ls\nunicode: %ls",
                         fi.GetVendor().c_str(),
                         fi.GetURL().c_str(),
                         fi.GetEmail().c_str(),
                         fi.IsDiscardable() ? L"yes" : L"no",
                         fi.IsLicenseCheck() ? L"yes" : L"no",
                         fi.IsComponentNonDiscardable() ? L"yes" : L"no",
                         fi.IsUnicode() ? L"yes" : L"no"
                         ).ToStdWstring();
        tc_factory_info_->Clear();
        tc_factory_info_->AppendText(fi_str.c_str());
        tc_factory_info_->ShowPosition(0);
        
        auto const num = factory->GetComponentCount();
        
        cho_select_component_->Clear();
        
        for(int i = 0; i < num; ++i) {
            auto const &info = factory->GetComponentInfo(i);
            
            HWM_DEBUG_LOG(info.GetName() << L", " << info.GetCategory());
            
            //! カテゴリがkVstAudioEffectClassなComponentを探索する。
            if(info.GetCategory() == hwm::to_wstr(kVstAudioEffectClass)) {
                cho_select_component_->Append(info.GetName(), new ComponentData{info.GetCID()});
            }
        }
        
        cho_select_component_->Show();
        
        if(cho_select_component_->GetCount() == 0) {
            cho_select_component_->SetSelection(wxNOT_FOUND);
            cho_select_component_->Disable();
        } else if(cho_select_component_->GetCount() == 1) {
            cho_select_component_->SetSelection(0);
            cho_select_component_->Disable();
            OnSelectComponent();
        } else {
            cho_select_component_->Enable();
            cho_select_component_->SetSelection(wxNOT_FOUND);
        }
        
        Layout();
    }
    
    void OnBeforeModuleUnloaded(Vst3PluginFactory *factory) override
    {
        tc_filepath_->Clear();
        
        vbox_factory_->ShowItems(false);
        cho_select_component_->Hide();
    }
    
    void OnAfterPluginLoaded(Vst3Plugin *plugin) override
    {
        UInt32 component_index = -1;
        auto cid = plugin->GetComponentInfo().GetCID();
        for(int i = 0; i < cho_select_component_->GetCount(); ++i) {
            auto component_data = static_cast<ComponentData *>(cho_select_component_->GetClientObject(i));
            if(component_data->cid_ == cid) {
                component_index = i;
                break;
            }
        }
        
        assert(component_index != -1);
        
        auto const &info = App::GetInstance()->GetPluginFactory()->GetComponentInfo(component_index);
        char cid_str[64] = {};
        auto fuid = Steinberg::FUID::fromTUID(info.GetCID().data());
        fuid.toRegistryString(cid_str);
    
        auto str = wxString::Format(L"Class ID: %s\ncategory: %ls\ncardinality: %d",
                                    cid_str,
                                    info.GetCategory().c_str(),
                                    info.GetCardinality());
        
        if(info.HasClassInfo2()) {
            auto info2 = info.GetClassInfo2();
            str += L"\n";
            str += wxString::Format(L"sub categories: %ls\nvendor: %ls\nversion: %ls\nsdk_version: %ls",
                                    info2.GetSubCategories().c_str(),
                                    info2.GetVendor().c_str(),
                                    info2.GetVersion().c_str(),
                                    info2.GetSDKVersion().c_str());
        }
        
        tc_component_info_->Clear();
        tc_component_info_->AppendText(str);
        tc_component_info_->ShowPosition(0);
    
        dummy_component_->Show(false);
        vbox_component_->ShowItems(true);
        btn_open_editor_->Show();
        btn_open_editor_->Enable();
        
        Layout();
    }
    
    void OnBeforePluginUnloaded(Vst3Plugin *plugin) override
    {
        OnCloseEditor();
        dummy_component_->Show(true);
        vbox_component_->ShowItems(false);
        btn_open_editor_->Hide();
    }
    
    wxTimer timer_;
    wxTextCtrl      *tc_filepath_;
    wxButton        *btn_load_module_;
    wxBoxSizer      *vbox_factory_;
    wxStaticText    *st_factory_info_label_;
    wxTextCtrl      *tc_factory_info_;
    wxChoice        *cho_select_component_;
    wxBoxSizer      *vbox_component_;
    wxSizerItem     *dummy_component_;
    wxStaticText    *st_component_info_label_;
    wxTextCtrl      *tc_component_info_;
    wxButton        *btn_open_editor_;
    wxWindow        *keyboard_;
    wxFrame         *editor_frame_ = nullptr;
    wxPanel         *header_panel_ = nullptr;
    
    ScopedListenerRegister<App::IModuleLoadListener> slr_mll_;
    ScopedListenerRegister<App::IPluginLoadListener> slr_pll_;
    
    void OnPaint(wxPaintEvent &)
    {
        Int32 kDotPeriod = 10;
        
        wxPaintDC pdc(this);
        wxGCDC dc(pdc);
        
        auto size = GetSize();
        
        dc.SetPen(wxPen(HSVToColour(0.0, 0.0, 0.9, 0.2)));
        for(int x = kDotPeriod; x < size.x; x += kDotPeriod) {
            for(int y = kDotPeriod; y < size.y; y += kDotPeriod) {
                dc.DrawPoint(x, y);
            }
        }
    }
    
    void OnLoadModule()
    {
        auto app = App::GetInstance();
        auto dir = app->GetConfig().plugin_search_path_;
        if(wxDir::Exists(dir) == false) {
#if defined(_MSC_VER)
            dir = L"C:/Program Files/Common Files/VST3";
#else
            dir = L"/Library/Audio/Plug-Ins/VST3";
#endif
        }
        
        // load
        wxFileDialog openFileDialog(this,
                                    "Open VST3 file",
                                    dir,
                                    "",
                                    "VST3 files (*.vst3)|*.vst3",
                                    wxFD_OPEN|wxFD_FILE_MUST_EXIST);

        if (openFileDialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        
        auto path = String(openFileDialog.GetPath().ToStdWstring());
        app->GetConfig().plugin_search_path_ = wxFileName(path).GetPath();
        app->SaveConfig();
        
        if(App::GetInstance()->LoadVst3Module(path) == false) {
            wxMessageBox(L"モジュールのオープンに失敗しました");
        }
    }
    
    class ComponentData : public wxClientData
    {
    public:
        ComponentData(ClassInfo::CID cid)
        :   cid_(cid)
        {}
        
        ClassInfo::CID cid_;
    };
    
    void OnSelectComponent() {
        auto sel = cho_select_component_->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        auto const p = static_cast<ComponentData const *>(cho_select_component_->GetClientObject(sel));

        bool found = false;
        auto factory = App::GetInstance()->GetPluginFactory();
        for(UInt32 i = 0, end = factory->GetComponentCount(); i < end; ++i) {
            if(factory->GetComponentInfo(i).GetCID() == p->cid_) {
                break;
            }
        }
        
        if(!found) { }
        App::GetInstance()->LoadVst3Plugin(p->cid_);
    }
    
    void OnDestroyFromPluginEditorFrame() override
    {
        editor_frame_ = nullptr;
        btn_open_editor_->Enable();
    }

public:
    bool CanOpenEditor() const
    {
        return btn_open_editor_->IsShown() && btn_open_editor_->IsEnabled();
    }
    
    void OnOpenEditor()
    {
        if(editor_frame_) { return; }
        if(CanOpenEditor() == false) { return; }

        editor_frame_ = CreatePluginEditorFrame(this,
                                                App::GetInstance()->GetPlugin(),
                                                this);
        btn_open_editor_->Disable();
    }
    
    void OnCloseEditor()
    {
        if(!editor_frame_) { return; }
        
        editor_frame_->Destroy();
        assert(editor_frame_ == nullptr);
    }
};

template<class... Args>
IMainFrame::IMainFrame(Args&&... args)
:   wxFrame(std::forward<Args>(args)...)
{
}

class MainFrame
:   public IMainFrame
,   public App::IPlaybackOptionChangeListener
{
    wxSize const initial_size = { 480, 600 };
    
    using base_type = IMainFrame;
public:
    MainFrame()
    :   base_type(nullptr, wxID_ANY, L"Vst3SampleHost", wxDefaultPosition, initial_size)
    {
#if defined(_MSC_VER)
        auto icon = wxIcon("IDI_ICON1", wxBITMAP_TYPE_ICO_RESOURCE, 32, 32);
        SetIcon(icon);
#endif

        SetMinClientSize(wxSize(350, 600));
       
        auto menu_file = new wxMenu();

        menu_file->Append(kID_File_Load, L"開く\tCTRL-O", L"プロジェクトファイルを開きます");
        menu_file->Append(kID_File_Save, L"保存\tCTRL-S", L"プロジェクトファイルを保存します");

        auto menu_playback = new wxMenu();
        menu_enable_input_ = menu_playback->AppendCheckItem(kID_Playback_EnableAudioInputs,
                                                            L"オーディオ入力を有効化\tCTRL-I",
                                                            L"オーディオ入力を有効にします");
        
        auto menu_waveform = new wxMenu();
        menu_waveform->AppendRadioItem(kID_Playback_Waveform_Sine, L"サイン波");
        menu_waveform->AppendRadioItem(kID_Playback_Waveform_Saw, L"のこぎり波");
        menu_waveform->AppendRadioItem(kID_Playback_Waveform_Square, L"矩形波");
        menu_waveform->AppendRadioItem(kID_Playback_Waveform_Triangle, L"三角波");

        menu_playback->AppendSeparator();
        menu_playback->AppendSubMenu(menu_waveform, L"テスト波形のタイプ");
        
        auto menu_view = new wxMenu();
        menu_view->Append(kID_View_PluginEditor, L"プラグインエディターを開く...\tCTRL-E", L"プラグインエディターを開きます");
        
        auto menu_device = new wxMenu();
        menu_device->Append(kID_Device_Preferences, L"デバイス設定\tCTRL-,", L"デバイス設定を変更します");
        
#if defined(_MSC_VER)
        auto menu_help = new wxMenu();   
        menu_help->Append(wxID_ABOUT, L"&Vst3SampleHostについて...", L"Vst3SampleHostについて");

        menu_file->AppendSeparator();
        menu_file->Append(wxID_EXIT, L"&Quit\tCTRL-Q", L"アプリケーションを終了します");
#else
        menu_file->Append(wxID_ABOUT, L"&Vst3SampleHostについて...", L"Vst3SampleHostについて");
        menu_file->Append(wxID_EXIT, L"&Quit\tCTRL-Q", L"アプリケーションを終了します");
#endif

        auto menubar = new wxMenuBar();
        menubar->Append(menu_file, L"ファイル");
        menubar->Append(menu_playback, L"再生");
        menubar->Append(menu_view, L"表示");
        menubar->Append(menu_device, L"デバイス");
#if defined(_MSC_VER)
        menubar->Append(menu_help, L"ヘルプ");
#endif
        SetMenuBar(menubar);

#if defined(_MSC_VER)
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto&) {
            Close();
        }, wxID_EXIT);
#endif
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &) {
            auto app = App::GetInstance();
            app->ShowAboutDialog();
        }, wxID_ABOUT);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &) { OnLoadProject(); }, kID_File_Load);
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &) { OnSaveProject(); }, kID_File_Save);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            auto app = App::GetInstance();
            app->EnableAudioInput(app->IsAudioInputEnabled() == false);
        }, kID_Playback_EnableAudioInputs);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(OscillatorType::kSine);
        }, kID_Playback_Waveform_Sine);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(OscillatorType::kSaw);
        }, kID_Playback_Waveform_Saw);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(OscillatorType::kSquare);
        }, kID_Playback_Waveform_Square);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(OscillatorType::kTriangle);
        }, kID_Playback_Waveform_Triangle);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            auto app = App::GetInstance();
            app->SelectAudioDevice();
        }, kID_Device_Preferences);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &ev) {
            OnOpenEditor();
        }, kID_View_PluginEditor);
        
        Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent &ev) {
            auto const app = App::GetInstance();
            auto const wt = app->GetTestWaveformType();
            
            if( (ev.GetId() == kID_Playback_Waveform_Sine && wt == OscillatorType::kSine) ||
                (ev.GetId() == kID_Playback_Waveform_Saw && wt == OscillatorType::kSaw) ||
                (ev.GetId() == kID_Playback_Waveform_Square && wt == OscillatorType::kSquare) ||
                (ev.GetId() == kID_Playback_Waveform_Triangle && wt == OscillatorType::kTriangle)
               )
            {
                ev.Check(true);
            } else {
                ev.Check(false);
            }
        }, kID_Playback_Waveform_Sine, kID_Playback_Waveform_Triangle);
        
        Bind(wxEVT_UPDATE_UI, [this](auto &ev) {
            ev.Enable(wnd_->CanOpenEditor());
        }, kID_View_PluginEditor);
        
        auto key_input = PCKeyboardInput::GetInstance();
        key_input->ApplyTo(this);
        
        SetClientSize(initial_size);
        wnd_ = new MainWindow(this);
        wnd_->Show();
        
        auto sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(wnd_, wxSizerFlags(1).Expand());
        
        SetSizer(sizer);
        
        project_file_dir_ = wxStandardPaths::Get().GetDocumentsDir().ToStdWstring();
        
        auto app = App::GetInstance();
        slr_pocl_.reset(app->GetPlaybackOptionChangeListenerService(), this);
        
        Bind(wxEVT_CLOSE_WINDOW, [this](auto &ev) {
            wnd_->Destroy();
            wnd_ = nullptr;
            Destroy();
        });
    }
    
    bool Destroy() override
    {
        DestroyChildren();
        wnd_ = nullptr;
        slr_pocl_.reset();
        return base_type::Destroy();
    }
    
private:
    MainWindow *wnd_;
    ScopedListenerRegister<App::IPlaybackOptionChangeListener> slr_pocl_;
    wxMenuItem *menu_enable_input_;
    String project_file_dir_;
    
    void OnAudioInputAvailabilityChanged(bool available) override
    {
        menu_enable_input_->Enable(available);
    }
    
    void OnAudioInputEnableStateChanged(bool enabled) override
    {
        menu_enable_input_->Check(enabled);
    }
    
    void OnLoadProject()
    {
        // load
        wxFileDialog openFileDialog(this,
                                    "Open Project file",
                                    project_file_dir_,
                                    "",
                                    "Vst3SampleHost Project files (*.vst3proj)|*.vst3proj",
                                    wxFD_OPEN|wxFD_FILE_MUST_EXIST);
        
        if (openFileDialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        
        auto path = String(openFileDialog.GetPath().ToStdWstring());
        project_file_dir_ = wxFileName(path).GetPath();
        
        auto app = App::GetInstance();
        app->LoadProjectFile(path);
    }
    
    void OnSaveProject()
    {
        // load
        wxFileDialog saveFileDialog(this,
                                    "Save Project file",
                                    project_file_dir_,
                                    "",
                                    "Vst3SampleHost Project files (*.vst3proj)|*.vst3proj",
                                    wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
        
        if (saveFileDialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        
        auto path = String(saveFileDialog.GetPath().ToStdWstring());
        project_file_dir_ = wxFileName(path).GetPath();
        
        auto app = App::GetInstance();
        app->SaveProjectFile(path);
    }
    
    void OnOpenEditor()
    {
        wnd_->OnOpenEditor();
    }
};

IMainFrame * CreateMainFrame()
{
    return new MainFrame();
}

NS_HWM_END
