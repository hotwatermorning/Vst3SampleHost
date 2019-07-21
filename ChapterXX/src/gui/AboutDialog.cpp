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

class AboutDialogPanel
:   public wxWindow
{
public:
    AboutDialogPanel(wxWindow *parent)
    :   wxWindow()
    {
#if defined(_MSC_VER)
        Create(parent, wxID_ANY);
        SetWindowLong(GetHWND(), GWL_EXSTYLE, GetWindowLong(GetHWND(), GWL_EXSTYLE) | WS_EX_LAYERED);
        int const font_point = 16;
        font_ = wxFont(wxFontInfo(font_point).Family(wxFONTFAMILY_TELETYPE).FaceName("Arial").AntiAliased(true));
#else
        SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
        Create(parent, wxID_ANY);
        int const font_point = 16;
        font_ = wxFont(wxFontInfo(font_point).Family(wxFONTFAMILY_MODERN).FaceName("Arial"));
#endif
        SetFocus();
        
        image_ = GetResourceAs<wxImage>(image_path);
        
        SetSize(image_.GetWidth(), image_.GetHeight());
        
        back_buffer_ = GraphicsBuffer(GetSize());
        
        //Bind(wxEVT_CLOSE_WINDOW, [this](auto &ev) { StartClosingWithEffect(); });
        Bind(wxEVT_PAINT, [this](auto &ev) { OnPaint(); });
        Bind(wxEVT_LEFT_UP, [this](auto &ev) { GetParent()->Close(true); });
        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &ev) {
            OnKeyDown(ev);
        });
        
        Show(true);
    }
    
    void OnKeyDown(wxKeyEvent &ev)
    {
        auto const uc = ev.GetUnicodeKey();
        auto const ac = ev.GetKeyCode();
        if(uc != wxKEY_NONE) {
            if(uc == WXK_RETURN || uc == WXK_ESCAPE) { GetParent()->Close(); }
        } else {
            if(ac == WXK_RETURN || ac == WXK_ESCAPE) { GetParent()->Close(); }
        }
    }
    
    bool IsOk() const
    {
        return image_.IsOk();
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
    
    wxImage image_;
    wxFont font_;
    GraphicsBuffer back_buffer_;
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
        panel_ = new AboutDialogPanel(this);
        SetClientSize(panel_->GetClientSize());
        
        CentreOnScreen();
    }
    
    bool IsOk() const override
    {
        return panel_->IsOk();
    }
    
private:
    AboutDialogPanel *panel_;
};

IAboutDialog * CreateAboutDialog()
{
    return new AboutDialog();
}

NS_HWM_END
