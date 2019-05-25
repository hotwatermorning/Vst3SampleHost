#include "Gui.hpp"
#include "../App.hpp"

#include <wx/filename.h>

#include "../resource/ResourceHelper.hpp"
#include "../plugin/vst3/Vst3Plugin.hpp"
#include "../plugin/vst3/Vst3PluginFactory.hpp"
#include "./Util.hpp"
#include "./Keyboard.hpp"
#include "./PluginEditor.hpp"
#include "./PCKeyboardInput.hpp"

NS_HWM_BEGIN

class HeaderPanel
:   public wxPanel
,   public App::PlaybackOptionChangeListener
{
public:
    wxColour const kColLabel = HSVToColour(0.0, 0.0, 0.9);
    wxColour const kColOutputSlider = HSVToColour(0.7, 0.4, 0.4);
    wxColour const kColInputCheckBox = HSVToColour(0.5, 0.4, 0.4);
    double const kVolumeSliderScale = 1000.0;
    
    HeaderPanel(wxWindow *parent)
    :   wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    ,   col_bg_(10, 10, 10)
    {
        auto app = App::GetInstance();
        auto min_value = app->GetAudioOutputMinLevel();
        auto max_value = app->GetAudioOutputMaxLevel();
        
        lbl_volume_ = new wxStaticText(this, wxID_ANY, L"出力レベル");
        lbl_volume_->SetForegroundColour(kColLabel);
        lbl_volume_->SetBackgroundColour(kColOutputSlider);
        
        sl_volume_ = new wxSlider(this, wxID_ANY, max_value * kVolumeSliderScale, min_value * kVolumeSliderScale, max_value * kVolumeSliderScale);
        sl_volume_->SetBackgroundColour(kColOutputSlider);
        sl_volume_->SetLabel(L"出力レベル");
        
        btn_enable_input_ = new wxCheckBox(this, wxID_ANY, L"マイク入力",
                                       wxDefaultPosition,
                                       wxSize(100, 1));
        btn_enable_input_->SetForegroundColour(kColLabel);
        btn_enable_input_->SetBackgroundColour(kColInputCheckBox);
        
        btn_enable_input_->Enable(app->CanEnableAudioInput());
        btn_enable_input_->SetValue(app->IsAudioInputEnabled());

        auto hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->Add(lbl_volume_, wxSizerFlags(0).Expand());
        hbox->Add(sl_volume_, wxSizerFlags(3).Expand().Border(wxRIGHT, 5));
        hbox->AddStretchSpacer(1);
        hbox->Add(btn_enable_input_, wxSizerFlags(0).Expand());
        
        auto vbox = new wxBoxSizer(wxVERTICAL);
        vbox->Add(hbox, wxSizerFlags(1).Expand().Border(wxALL, 5));
        SetSizer(vbox);
        
        sl_volume_->Bind(wxEVT_SLIDER, [this](wxCommandEvent &ev) { OnSlider(ev); });
        btn_enable_input_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &ev) { OnCheckBox(ev); });

        slr_pocl_.reset(app->GetPlaybackOptionChangeListenerService(), this);
        SetBackgroundColour(col_bg_);
    }
    
private:
    wxStaticText *lbl_volume_;
    wxSlider *sl_volume_;
    wxCheckBox *btn_enable_input_;
    wxColor col_bg_;
    ScopedListenerRegister<App::PlaybackOptionChangeListener> slr_pocl_;
    
private:
    void OnSlider(wxCommandEvent &ev)
    {
        auto app = App::GetInstance();
        app->SetAudioOutputLevel(sl_volume_->GetValue() / kVolumeSliderScale);
    }
    
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

class MainWindow
:   public wxWindow
,   public App::ModuleLoadListener
,   public App::PluginLoadListener
,   public PluginEditorFrameListener
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
        
        st_filepath_ = new wxStaticText(this, 100, "", wxDefaultPosition, wxSize(100, 20), wxST_NO_AUTORESIZE);
        st_filepath_->SetBackgroundColour(*wxWHITE);
        st_filepath_->Disable();
        
        btn_load_module_ = new wxButton(this, 101, "Load Module", wxDefaultPosition, wxSize(100, 20));
        
        cho_select_component_ = new wxChoice(this, 102, wxDefaultPosition, wxSize(100, 20));
        cho_select_component_->Hide();
        
        st_class_info_ = new wxStaticText(this, 103, "", wxDefaultPosition, wxSize(100, 100), wxST_NO_AUTORESIZE);
        st_class_info_->SetBackgroundColour(*wxWHITE);
        st_class_info_->Hide();
        
        cho_select_unit_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(100, 20));
        cho_select_unit_->Hide();
        
        cho_select_program_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(100, 20));
        cho_select_program_->Hide();
        
        btn_open_editor_ = new wxButton(this, wxID_ANY, "Open Editor", wxDefaultPosition, wxSize(100, 20));
        btn_open_editor_->Hide();
        
        keyboard_ = CreateKeyboardPanel(this);
        
        auto hbox1 = new wxBoxSizer(wxHORIZONTAL);
        
        hbox1->Add(st_filepath_, wxSizerFlags(1).Centre().Border(wxRIGHT));
        hbox1->Add(btn_load_module_, wxSizerFlags().Centre().Border(wxLEFT));
        
        auto hbox2 = new wxBoxSizer(wxHORIZONTAL);
        
        auto vbox2 = new wxBoxSizer(wxVERTICAL);
        vbox2->Add(cho_select_component_, wxSizerFlags(0).Expand().Border(wxBOTTOM));
        vbox2->Add(st_class_info_, wxSizerFlags(1).Expand().Border(wxTOP));
        
        auto vbox3 = new wxBoxSizer(wxVERTICAL);
        vbox3->Add(btn_open_editor_, wxSizerFlags(0).Border(wxBOTTOM));
        
        unit_param_box_ = new wxStaticBoxSizer(wxVERTICAL, this, "Units && Programs");
        unit_param_box_->GetStaticBox()->Hide();
        unit_param_box_->GetStaticBox()->SetForegroundColour(wxColour(0xFF, 0xFF, 0xFF));
        
        unit_param_box_->Add(cho_select_unit_, wxSizerFlags(0).Expand().Border(wxBOTTOM|wxTOP, 2));
        unit_param_box_->Add(cho_select_program_, wxSizerFlags(0).Expand().Border(wxBOTTOM|wxTOP, 2));
        
        vbox3->Add(unit_param_box_, wxSizerFlags(0).Expand().Border(wxBOTTOM|wxTOP));
        vbox3->AddStretchSpacer();
        
        hbox2->Add(vbox2, wxSizerFlags(1).Expand().Border(wxRIGHT));
        hbox2->Add(vbox3, wxSizerFlags(1).Expand().Border(wxLEFT));
        
        auto vbox = new wxBoxSizer(wxVERTICAL);
        vbox->Add(header_panel_, wxSizerFlags(0).Expand());
        vbox->Add(hbox1, wxSizerFlags(0).Expand().Border(wxALL, 10));
        vbox->Add(hbox2, wxSizerFlags(1).Expand().Border(wxALL, 10));
        vbox->Add(keyboard_, wxSizerFlags(0).Expand());
        
        SetSizerAndFit(vbox);
        
        Bind(wxEVT_PAINT, [this](auto &ev) { OnPaint(ev); });
        btn_load_module_->Bind(wxEVT_BUTTON, [this](auto &ev) { OnLoadModule(); });
        cho_select_component_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectComponent(); });
        btn_open_editor_->Bind(wxEVT_BUTTON, [this](auto &ev) { OnOpenEditor(); });
        cho_select_unit_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectUnit(); });
        cho_select_program_->Bind(wxEVT_CHOICE, [this](auto &ev) { OnSelectProgram(); });
        
        auto app = App::GetInstance();
        slr_mll_.reset(app->GetModuleLoadListenerService(), this);
        slr_pll_.reset(app->GetPluginLoadListenerService(), this);
    }
    
    bool Destroy() override {
        OnCloseEditor();
        return wxWindow::Destroy();
    }
    
private:
    
    void OnAfterModuleLoaded(String path, Vst3PluginFactory *factory) override
    {
        st_filepath_->SetLabel(path.c_str());
        
        auto const num = factory->GetComponentCount();
        
        cho_select_component_->Clear();
        for(int i = 0; i < num; ++i) {
            auto const &info = factory->GetComponentInfo(i);
            
            hwm::wdout << info.name() << L", " << info.category() << std::endl;
            
            //! カテゴリがkVstAudioEffectClassなComponentを探索する。
            if(info.category() == hwm::to_wstr(kVstAudioEffectClass)) {
                cho_select_component_->Append(info.name(), new ComponentData{info.cid()});
            }
        }
        
        if(cho_select_component_->GetCount() == 0) {
            return;
        } else if(cho_select_component_->GetCount() == 1) {
            cho_select_component_->SetSelection(0);
            cho_select_component_->Disable();
            OnSelectComponent();
        } else {
            cho_select_component_->Enable();
            cho_select_component_->SetSelection(wxNOT_FOUND);
        }
        
        cho_select_component_->Show();
        Layout();
    }
    
    void OnBeforeModuleUnloaded(Vst3PluginFactory *factory) override
    {
        st_filepath_->SetLabel("");
        
        cho_select_component_->Hide();
    }
    
    void OnAfterPluginLoaded(Vst3Plugin *plugin) override
    {
        UInt32 component_index = -1;
        auto cid = plugin->GetComponentInfo().cid();
        for(int i = 0; i < cho_select_component_->GetCount(); ++i) {
            auto component_data = static_cast<ComponentData *>(cho_select_component_->GetClientObject(i));
            if(component_data->cid_ == cid) {
                component_index = i;
                break;
            }
        }
        
        assert(component_index != -1);
        
        auto const &info = App::GetInstance()->GetPluginFactory()->GetComponentInfo(component_index);
        auto str = wxString::Format(L"[%s]\ncategory: ", info.name(), info.category());
        
        if(info.has_classinfo2()) {
            auto info2 = info.classinfo2();
            str += L"\n";
            str += wxString::Format(L"sub categories: %s\nvendor: %s\nversion: %s\nsdk_version: %s",
                                    info2.sub_categories(),
                                    info2.vendor(),
                                    info2.version(),
                                    info2.sdk_version());
            st_class_info_->SetLabel(str);
        }
        
        cho_select_unit_->Clear();
        cho_select_program_->Clear();
        
        auto const num = plugin->GetNumUnitInfo();
        for(int un = 0; un < num; ++un) {
            auto const &info = plugin->GetUnitInfoByIndex(un);
            auto const &pl = info.program_list_;
            if(info.program_change_param_ == Steinberg::Vst::kNoParamId) { continue; }
            if(pl.id_ == Steinberg::Vst::kNoProgramListId) { continue; }
            if(pl.programs_.empty()) { continue; }
            
            cho_select_unit_->Append(info.name_, new UnitData{info.id_});
        }
        
        if(cho_select_unit_->GetCount() == 0) {
            //nothing to do for cho_select_unit_ and cho_select_program_
        } else if(cho_select_unit_->GetCount() == 1) {
            unit_param_box_->GetStaticBox()->Show();
            cho_select_unit_->SetSelection(0);
            cho_select_unit_->Show();
            cho_select_unit_->Disable();
            OnSelectUnit();
        } else {
            unit_param_box_->GetStaticBox()->Show();
            cho_select_unit_->SetSelection(0);
            cho_select_unit_->Show();
            cho_select_unit_->Enable();
            OnSelectUnit();
        }

        st_class_info_->Show();
        btn_open_editor_->Show();
        btn_open_editor_->Enable();
        
        Layout();
    }
    
    void OnBeforePluginUnloaded(Vst3Plugin *plugin) override
    {
        OnCloseEditor();
        st_class_info_->Hide();
        btn_open_editor_->Hide();
        cho_select_unit_->Hide();
        cho_select_program_->Hide();
        unit_param_box_->GetStaticBox()->Hide();
    }
    
    wxTimer timer_;
    wxStaticBoxSizer *unit_param_box_;
    wxStaticText    *st_filepath_;
    wxButton        *btn_load_module_;
    wxChoice        *cho_select_component_;
    wxStaticText    *st_class_info_;
    wxChoice        *cho_select_unit_;
    wxChoice        *cho_select_program_;
    wxButton        *btn_open_editor_;
    wxWindow        *keyboard_;
    wxFrame         *editor_frame_ = nullptr;
    wxPanel         *header_panel_ = nullptr;
    String          module_dir_;
    
    ScopedListenerRegister<App::ModuleLoadListener> slr_mll_;
    ScopedListenerRegister<App::PluginLoadListener> slr_pll_;
    
    void OnPaint(wxPaintEvent &)
    {
        wxPaintDC pdc(this);
        Draw(pdc);
    }
    
    void Draw(wxDC &dc)
    {
        //dc.SetBrush(wxBrush(wxColour(0x09, 0x21, 0x33)));
        //dc.DrawRectangle(GetClientRect());
    }
    
    void OnLoadModule()
    {
        if(wxFileName(module_dir_, "").DirExists() == false) {
#if defined(_MSC_VER)
            module_dir_ = L"C:/Program Files/Common Files/VST3";
#else
            module_dir_ = L"/Library/Audio/Plug-Ins/VST3";
#endif
        }
        
        // load
        wxFileDialog openFileDialog(this,
                                    "Open VST3 file",
                                    module_dir_,
                                    "",
                                    "VST3 files (*.vst3)|*.vst3",
                                    wxFD_OPEN|wxFD_FILE_MUST_EXIST);

        if (openFileDialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        
        auto path = String(openFileDialog.GetPath().ToStdWstring());
        module_dir_ = wxFileName(path).GetPath();
        
        App::GetInstance()->LoadVst3Module(path);
    }
    
    class ComponentData : public wxClientData
    {
    public:
        ComponentData(ClassInfo::CID cid)
        :   cid_(cid)
        {}
        
        ClassInfo::CID cid_;
    };
    
    class UnitData : public wxClientData
    {
    public:
        UnitData(Steinberg::Vst::UnitID unit_id)
        :   unit_id_(unit_id)
        {}
        
        Steinberg::Vst::UnitID unit_id_ = -1;
    };
    
    void OnSelectComponent() {
        auto sel = cho_select_component_->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        auto const p = static_cast<ComponentData const *>(cho_select_component_->GetClientObject(sel));

        bool found = false;
        auto factory = App::GetInstance()->GetPluginFactory();
        for(UInt32 i = 0, end = factory->GetComponentCount(); i < end; ++i) {
            if(factory->GetComponentInfo(i).cid() == p->cid_) {
                break;
            }
        }
        
        if(!found) { }
        App::GetInstance()->LoadVst3Plugin(p->cid_);
    }
    
    Vst3Plugin::UnitID GetCurrentUnitID() const
    {
        auto const sel = cho_select_unit_->GetSelection();
        if(sel == wxNOT_FOUND) { return -1; }
        
        assert(cho_select_unit_->HasClientObjectData());
        auto data = static_cast<UnitData *>(cho_select_unit_->GetClientObject(sel));
        return data->unit_id_;
    }
    
    void OnDestroyFromPluginEditorFrame() override
    {
        editor_frame_ = nullptr;
        btn_open_editor_->Enable();
    }
    
    void OnOpenEditor()
    {
        if(editor_frame_) { return; }

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
    
    void OnSelectUnit()
    {
        auto sel = cho_select_unit_->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        auto unit_id = GetCurrentUnitID();
        assert(unit_id != -1);
        
        auto plugin = App::GetInstance()->GetPlugin();
        auto info = plugin->GetUnitInfoByID(unit_id);
        
        assert(info.program_list_.programs_.size() >= 1);
        
        // update program list
        cho_select_program_->Clear();
        for(UInt32 i = 0; i < info.program_list_.programs_.size(); ++i) {
            cho_select_program_->Append(info.program_list_.programs_[i].name_);
        }
        
        cho_select_program_->Select(plugin->GetProgramIndex(unit_id));
        cho_select_program_->Show();
        Layout();
    }
    
    void OnSelectProgram()
    {
        auto sel = cho_select_program_->GetSelection();
        if(sel == wxNOT_FOUND) { return; }
        
        assert(cho_select_unit_->GetSelection() != wxNOT_FOUND);
        
        auto const unit_id = GetCurrentUnitID();
        assert(unit_id != -1);
        
        auto plugin = App::GetInstance()->GetPlugin();
        auto info = plugin->GetUnitInfoByID(unit_id);
        
        plugin->SetProgramIndex(sel, unit_id);
    }
};

class MainFrame
:   public wxFrame
{
    wxSize const initial_size = { 600, 400 };
    
    enum {
        kID_Playback_EnableAudioInputs = wxID_HIGHEST + 1,
        kID_Device_Preferences
    };
    
public:
    MainFrame()
    :   wxFrame(nullptr, wxID_ANY, L"Vst3SampleHost", wxDefaultPosition, initial_size)
    {
        SetMinClientSize(wxSize(350, 320));
        
        auto menu_playback = new wxMenu();
        menu_playback->AppendCheckItem(kID_Playback_EnableAudioInputs, L"オーディオ入力を有効化\tCTRL-I", L"オーディオ入力を有効にします");
        
        auto menu_device = new wxMenu();
        menu_device->Append(kID_Device_Preferences, L"デバイス設定\tCTRL-,", L"デバイス設定を変更します");
        
        auto menubar = new wxMenuBar();
        menubar->Append(menu_playback, L"再生");
        menubar->Append(menu_device, L"デバイス");
        SetMenuBar(menubar);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            auto app = App::GetInstance();
            app->EnableAudioInput(app->IsAudioInputEnabled() == false);
        }, kID_Playback_EnableAudioInputs);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            auto app = App::GetInstance();
            app->SelectAudioDevice();
        }, kID_Device_Preferences);
        
        auto key_input = PCKeyboardInput::GetInstance();
        key_input->ApplyTo(this);
        
        SetClientSize(initial_size);
        wnd_ = new MainWindow(this);
        wnd_->Show();
        
        auto sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(wnd_, wxSizerFlags(1).Expand());
        
        SetSizer(sizer);
        
        Bind(wxEVT_CLOSE_WINDOW, [this](auto &ev) {
            wnd_->Destroy();
            wnd_ = nullptr;
            Destroy();
        });
    }
    
private:
    wxWindow *wnd_;
};

wxFrame * CreateMainFrame()
{
    return new MainFrame();
}

NS_HWM_END
