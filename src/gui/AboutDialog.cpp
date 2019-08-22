#include "./AboutDialog.hpp"
#include "./Util.hpp"
#include "../resource/ResourceHelper.hpp"

#include <wx/hyperlink.h>

NS_HWM_BEGIN

namespace {
    String image_path = L"about_dialog.png";
}

template<class... Args>
IAboutDialog::IAboutDialog(Args&&... args)
:   wxFrame(std::forward<Args>(args)...)
{}

class AboutDialog
:   public IAboutDialog
{
    void OnKeyDown(wxKeyEvent &ev)
    {
        auto const uc = ev.GetUnicodeKey();
        auto const ac = ev.GetKeyCode();
        if(uc != wxKEY_NONE) {
            if(uc == WXK_RETURN || uc == WXK_ESCAPE) {
                Close();
            }
        } else {
            if(ac == WXK_RETURN || ac == WXK_ESCAPE) {
                Close();
            }
        }
        ev.ResumePropagation(0);
    }
    
    struct Panel : wxWindow
    {
        Panel(AboutDialog *parent, wxWindowID id, wxPoint pos, wxSize size)
        :   wxWindow(parent, id, pos, size, wxWANTS_CHARS)
        {
            Bind(wxEVT_KEY_DOWN, [parent](wxKeyEvent& ev) {
                parent->OnKeyDown(ev);
            });
        }
        
        bool AcceptsFocus() const override { return true; }
    };
    
    Panel *panel_ = nullptr;
    
public:
    AboutDialog()
    :   IAboutDialog()
    {
#if defined(_MSC_VER)
        Create(nullptr, wxID_ANY, kAppName, wxDefaultPosition, wxDefaultSize,
               wxSTAY_ON_TOP|wxWANTS_CHARS|wxFRAME_NO_TASKBAR|wxBORDER_NONE);
        SetWindowLong(GetHWND(), GWL_EXSTYLE, GetWindowLong(GetHWND(), GWL_EXSTYLE) | WS_EX_LAYERED);        
#else
        SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
        Create(nullptr, wxID_ANY, kAppName, wxDefaultPosition, wxDefaultSize,
               wxSTAY_ON_TOP|wxWANTS_CHARS|wxFRAME_NO_TASKBAR|wxBORDER_NONE);
#endif
        int const font_point = 16;
        font_ = wxFont(wxFontInfo(font_point)
                       .Family(wxFONTFAMILY_TELETYPE)
                       .FaceName("Arial")
                       .AntiAliased(true)
                       .Weight(wxFONTWEIGHT_NORMAL)
                       );
        font_underlined_ = font_.Underlined();
        
        image_ = GetResourceAs<wxImage>(image_path);
        if(image_.IsOk() == false) {
            return;
        }

        SetClientSize(image_.GetWidth(), image_.GetHeight());
        back_buffer_ = GraphicsBuffer(GetClientSize());
        
        panel_ = new Panel(this, wxID_ANY, wxDefaultPosition, image_.GetSize());
        
        by_ = "by ";
        url_ = "diatonic.jp";
        url_actual_ = "https://diatonic.jp";

        {
            wxClientDC dc(this);
            dc.SetFont(font_);
            auto sz_by = dc.GetTextExtent(by_);
            auto sz_url = dc.GetTextExtent(url_);
            
            rc_url_ = wxRect {
                wxPoint { image_.GetWidth() - padding_x_ - sz_url.x, link_offset_y_ },
                sz_url
            };
            
            rc_by_ = wxRect {
                wxPoint{ rc_url_.x - sz_by.x, link_offset_y_ },
                sz_by
            };
        }

        Bind(wxEVT_PAINT, [this](auto &) { OnPaint(); });
        
        Bind(wxEVT_LEFT_UP, [this](auto &ev) {
            OnLeftClicked(ev.GetPosition());
        });
        Bind(wxEVT_MOTION, [this](auto &ev) {
            OnMotion(ev);
        });
        
        SetFocus();
        CaptureMouse();
        
        CentreOnScreen();
    }
    
    bool AcceptsFocus() const override { return true; }
    
    bool Destroy() override
    {
        if(HasCapture()) {
            ReleaseMouse();
        }
        return IAboutDialog::Destroy();
    }
    
    bool IsOk() const override
    {
        return image_.IsOk();
    }
    
    void OnLeftClicked(wxPoint const &pt)
    {
        if(rc_url_.Contains(pt)) {
#if defined(_MSC_VER)
            system(("start " + url_actual_).c_str());
#else
            system(("open " + url_actual_).c_str());
#endif
        } else {
            Close();
        }
    }
    
    void OnMotion(wxMouseEvent const &ev)
    {
        auto last_hovering = is_hovering_;
        is_hovering_ = rc_url_.Contains(ev.GetPosition());
        if(last_hovering != is_hovering_) {
            Refresh();
        }
    }

    void OnPaint()
    {
        Render();
        
        wxMemoryDC memory_dc(back_buffer_.GetBitmap());
        assert(memory_dc.IsOk());
        
        wxPaintDC pdc(this);
        
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
        auto dest_hdc = pdc.GetHDC();
        
        UpdateLayeredWindow(hwnd, dest_hdc, nullptr, &size, src_hdc, &pt_src, 0, &bf, ULW_ALPHA);
#else
        pdc.Blit(wxPoint(), GetClientSize(), &memory_dc, wxPoint());
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
        dc.SetTextForeground(col_text_);
        dc.DrawLabel(by_, rc_by_, wxALIGN_CENTER_VERTICAL);

        dc.SetFont(font_underlined_);
        if(is_hovering_) {
            dc.SetTextForeground(col_link_hover_);
        } else {
            dc.SetTextForeground(col_link_);
        }
        dc.DrawLabel(url_, rc_url_, wxALIGN_CENTER_VERTICAL);

        dc.SetFont(font_);
        dc.SetTextForeground(col_text_);
        auto font_size = font_.GetPixelSize();
        
        auto text_rect = wxRect{
            wxPoint{ padding_x_, img.GetHeight() / 2 },
            wxSize { img.GetWidth() - padding_x_ * 2, font_size.GetHeight() }
        };
        
        auto const version_str =
        String(L"Version: ") + kAppVersion +
        L" (" + String(kAppCommitID).substr(0, 8) + L")";
        
        dc.DrawLabel(version_str, text_rect, wxALIGN_CENTER_VERTICAL);
    }
    
private:
    wxImage image_;
    wxFont font_;
    wxFont font_underlined_;
    GraphicsBuffer back_buffer_;
    wxRect rc_by_;
    wxRect rc_url_;
    wxColour col_text_          = *wxWHITE;
    wxColour col_link_          = *wxWHITE;
    wxColour col_link_hover_    = wxColour(150, 150, 255);
    std::string url_;
    std::string url_actual_;
    std::string by_;
    bool is_hovering_ = false;
    Int32 const padding_x_ = 62;
    Int32 const link_offset_y_ = 113;
};

std::unique_ptr<IAboutDialog, IAboutDialog::Destroyer> CreateAboutDialog()
{
    return std::unique_ptr<IAboutDialog, IAboutDialog::Destroyer>(new AboutDialog());
}

NS_HWM_END
