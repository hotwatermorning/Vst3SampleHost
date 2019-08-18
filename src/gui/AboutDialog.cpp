#include "./AboutDialog.hpp"
#include "./Util.hpp"
#include "../resource/ResourceHelper.hpp"

NS_HWM_BEGIN

namespace {
    String image_path = L"about_dialog.png";
}

template<class... Args>
IAboutDialog::IAboutDialog(Args&&... args)
:   wxDialog(std::forward<Args>(args)...)
{}

// 子ウィンドウを用意しないでwxDialogをそのまま使うとマウスイベントが正しくハンドリングできなかった。
// なので、マウスイベントを扱うためだけの仮のPanelを用意してそれを使用している。
class AboutDialogPanel
:   public wxWindow
{
public:
    AboutDialogPanel(wxWindow *parent = nullptr,
                     wxWindowID id = wxID_ANY,
                     wxPoint pos = wxDefaultPosition,
                     wxSize size = wxDefaultSize)
    :   wxWindow(parent, wxID_ANY, pos, size)
    {         
        Bind(wxEVT_LEFT_UP, [this](auto &ev) { GetParent()->Close(true); });
        Show(true);
    }
};

class AboutDialog
:   public IAboutDialog
{
public:
    AboutDialog()
    :   IAboutDialog()
    {
#if defined(_MSC_VER)
        Create(nullptr, wxID_ANY, kAppName, wxDefaultPosition, wxDefaultSize,
               wxFRAME_NO_TASKBAR | wxBORDER_NONE);
        SetWindowLong(GetHWND(), GWL_EXSTYLE, GetWindowLong(GetHWND(), GWL_EXSTYLE) | WS_EX_LAYERED);        
#else
        SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
        Create(nullptr, wxID_ANY, kAppName, wxDefaultPosition, wxDefaultSize,
               wxFRAME_NO_TASKBAR | wxBORDER_NONE);
#endif
        int const font_point = 16;
        font_ = wxFont(wxFontInfo(font_point).Family(wxFONTFAMILY_TELETYPE).FaceName("Arial").AntiAliased(true));

        image_ = GetResourceAs<wxImage>(image_path);
        if(image_.IsOk() == false) {
            return;
        }

        SetClientSize(image_.GetWidth(), image_.GetHeight());
        back_buffer_ = GraphicsBuffer(GetClientSize());
        panel_ = new AboutDialogPanel(this);
        panel_->SetFocus();
         
        auto sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(panel_, wxSizerFlags(1).Expand());
        SetSizer(sizer);

        Bind(wxEVT_PAINT, [this](auto &) { OnPaint(); }); 

        // 子ウィンドウのwxEVT_KEY_UPでは、WXK_RETURNがハンドリングできないので、親ウィンドウの側でwxEVT_CHAR_HOOKでキーイベントをハンドリングする
        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& ev) {
            OnKeyDown(ev);
        });
        
        CentreOnScreen();
    }
    
    bool IsOk() const override
    {
        return image_.IsOk();
    }

    void OnKeyDown(wxKeyEvent &ev)
    {
        auto const uc = ev.GetUnicodeKey();
        auto const ac = ev.GetKeyCode();
        if(uc != wxKEY_NONE) {
            if(uc == WXK_RETURN || uc == WXK_ESCAPE) { Close(); }
        } else {
            if(ac == WXK_RETURN || ac == WXK_ESCAPE) { Close(); }
        }
    }

    void OnPaint()
    {
        Render();
        
        wxMemoryDC memory_dc(back_buffer_.GetBitmap());
        assert(memory_dc.IsOk());
        
#if defined(_MSC_VER)
        auto hwnd = GetHWND();
        wxClientDC cdc(this);
        POINT pt_src{ 0, 0 };
        POINT pt_dest{ 0, 0 };
        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        SIZE size{ GetClientSize().GetWidth(), GetClientSize().GetHeight() };
        
        auto src_hdc = memory_dc.GetHDC();
        auto dest_hdc = cdc.GetHDC();
        
        UpdateLayeredWindow(hwnd, dest_hdc, nullptr, &size, src_hdc, &pt_src, 0, &bf, ULW_ALPHA);
#else
        wxPaintDC pdc(this);
        pdc.Blit(wxPoint(), GetClientSize(), &memory_dc, wxPoint());
        SetTransparent(255);
#endif
    }
    
    void Render()
    {
        back_buffer_.Clear();
        
        wxMemoryDC memory_dc(back_buffer_.GetBitmap());
        assert(memory_dc.IsOk());
        
        wxGCDC dc(memory_dc);
        
        auto img = wxBitmap(image_, 32);
        dc.DrawBitmap(img, wxPoint());
        
        dc.SetFont(font_);
        dc.SetTextForeground(*wxWHITE);
    }
    
private:
    AboutDialogPanel *panel_ = 0;
    wxImage image_;
    wxFont font_;
    GraphicsBuffer back_buffer_;
};

std::unique_ptr<IAboutDialog, Destroyer> CreateAboutDialog()
{
    return std::unique_ptr<IAboutDialog, Destroyer>(new AboutDialog());
}

NS_HWM_END
