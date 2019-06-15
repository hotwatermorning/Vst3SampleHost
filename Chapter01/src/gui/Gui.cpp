#include "Gui.hpp"
#include "../App.hpp"

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "../resource/ResourceHelper.hpp"
#include "./Util.hpp"
#include "./Keyboard.hpp"
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
    }
    
    bool AcceptsFocus() const override { return false; }
    
    bool Destroy() override
    {
        return wxWindow::Destroy();
    }
    
private:
    wxTimer timer_;
    wxTextCtrl      *tc_filepath_;
    wxButton        *btn_load_module_;
    wxWindow        *keyboard_;
    wxPanel         *header_panel_ = nullptr;
    String          module_dir_;
    
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
        
        auto menu_device = new wxMenu();
        menu_device->Append(kID_Device_Preferences, L"デバイス設定\tCTRL-,", L"デバイス設定を変更します");
        
        auto menubar = new wxMenuBar();
        menubar->Append(menu_file, L"ファイル");
        menubar->Append(menu_playback, L"再生");
        menubar->Append(menu_device, L"デバイス");
        
        SetMenuBar(menubar);
        
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &) { OnLoadProject(); }, kID_File_Load);
        Bind(wxEVT_COMMAND_MENU_SELECTED, [this](auto &) { OnSaveProject(); }, kID_File_Save);
        
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
};

IMainFrame * CreateMainFrame()
{
    return new MainFrame();
}

NS_HWM_END
