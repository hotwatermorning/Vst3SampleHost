#include "Gui.hpp"
#include "../App.hpp"

#include <wx/filename.h>
#include <wx/stdpaths.h>

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
        
        sl_volume_ = new wxSlider(this, wxID_ANY,
                                  app->GetAudioOutputLevel() * kVolumeSliderScale,
                                  min_value * kVolumeSliderScale,
                                  max_value * kVolumeSliderScale);
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
    
    bool AcceptsFocus() const override { return false; }
    
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
                         fi.vendor().c_str(),
                         fi.url().c_str(),
                         fi.email().c_str(),
                         fi.discardable() ? L"yes" : L"no",
                         fi.license_check() ? L"yes" : L"no",
                         fi.component_non_discardable() ? L"yes" : L"no",
                         fi.unicode() ? L"yes" : L"no"
                         ).ToStdWstring();
        tc_factory_info_->Clear();
        tc_factory_info_->AppendText(fi_str.c_str());
        tc_factory_info_->ShowPosition(0);
        
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
        char cid_str[64] = {};
        auto fuid = Steinberg::FUID::fromTUID(info.cid().data());
        fuid.toRegistryString(cid_str);
    
        auto str = wxString::Format(L"Class ID: %s\ncategory: %ls\ncardinality: %d",
                                    cid_str,
                                    info.category().c_str(),
                                    info.cardinality());
        
        if(info.has_classinfo2()) {
            auto info2 = info.classinfo2();
            str += L"\n";
            str += wxString::Format(L"sub categories: %ls\nvendor: %ls\nversion: %ls\nsdk_version: %ls",
                                    info2.sub_categories().c_str(),
                                    info2.vendor().c_str(),
                                    info2.version().c_str(),
                                    info2.sdk_version().c_str());
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
    String          module_dir_;
    
    ScopedListenerRegister<App::ModuleLoadListener> slr_mll_;
    ScopedListenerRegister<App::PluginLoadListener> slr_pll_;
    
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
,   public App::PlaybackOptionChangeListener
{
    wxSize const initial_size = { 480, 600 };
    
    using base_type = IMainFrame;
public:
    MainFrame()
    :   base_type(nullptr, wxID_ANY, L"Vst3SampleHost", wxDefaultPosition, initial_size)
    {
        SetMinClientSize(wxSize(350, 600));
       
        auto menu_file = new wxMenu();

        menu_file->Append(kID_File_Load, L"開く\tCTRL-O", L"プロジェクトファイルを開きます。");
        menu_file->Append(kID_File_Save, L"保存\tCTRL-S", L"プロジェクトファイルを保存します。");

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
        menu_view->Append(kID_View_PluginEditor, L"プラグインエディターを開く\tCTRL-E", L"プラグインエディターを開きます");
        
        auto menu_device = new wxMenu();
        menu_device->Append(kID_Device_Preferences, L"デバイス設定\tCTRL-,", L"デバイス設定を変更します");
        
        auto menubar = new wxMenuBar();
        menubar->Append(menu_file, L"ファイル");
        menubar->Append(menu_playback, L"再生");
        menubar->Append(menu_view, L"表示");
        menubar->Append(menu_device, L"デバイス");
        SetMenuBar(menubar);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &) { OnLoadProject(); }, kID_File_Load);
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &) { OnSaveProject(); }, kID_File_Save);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            auto app = App::GetInstance();
            app->EnableAudioInput(app->IsAudioInputEnabled() == false);
        }, kID_Playback_EnableAudioInputs);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(App::TestWaveformType::kSine);
        }, kID_Playback_Waveform_Sine);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(App::TestWaveformType::kSaw);
        }, kID_Playback_Waveform_Saw);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(App::TestWaveformType::kSquare);
        }, kID_Playback_Waveform_Square);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            App::GetInstance()->SetTestWaveformType(App::TestWaveformType::kTriangle);
        }, kID_Playback_Waveform_Triangle);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [](auto &ev) {
            auto app = App::GetInstance();
            app->SelectAudioDevice();
        }, kID_Device_Preferences);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &ev) {
            OnOpenEditor();
        }, kID_View_PluginEditor);
        
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
    ScopedListenerRegister<App::PlaybackOptionChangeListener> slr_pocl_;
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
