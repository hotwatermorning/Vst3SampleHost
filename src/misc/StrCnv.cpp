#include "StrCnv.hpp"
#include <cwchar>
#include <wx/wx.h>

NS_HWM_BEGIN

std::wstring to_wstr(std::wstring const &str)
{
    return str;
}

std::wstring to_wstr(wchar_t const *first, wchar_t const *last)
{
    return {first, last};
}

std::wstring to_wstr(std::string const &str)
{
    return to_wstr(str.c_str(), str.c_str() + str.size());
}

std::wstring to_wstr(char const *first, char const *last)
{
    return wxString(first, wxMBConvUTF8{}, last - first).ToStdWstring();
}

std::wstring to_wstr(std::u16string const &str)
{
    return to_wstr(str.c_str(), str.c_str() + str.size());
}

std::wstring to_wstr(char16_t const *first, char16_t const *last)
{
    std::vector<char> buf((last - first) * sizeof(char16_t));
    memcpy(buf.data(), first, buf.size());
    
    return wxString(buf.data(), wxMBConvUTF16LE{}).ToStdWstring();
}

std::string to_utf8(std::wstring const &str)
{
    return to_utf8(str.c_str(), str.c_str() + str.size());
}

std::string to_utf8(wchar_t const *first, wchar_t const *last)
{
    return wxString(first, last - first).ToStdString(wxMBConvUTF8{});
}

std::u16string to_utf16(std::wstring const &str)
{
    return to_utf16(str.c_str(), str.c_str() + str.size());
}

std::u16string to_utf16(wchar_t const *first, wchar_t const *last)
{
    auto ws = wxString(first, last - first);
    auto buf = ws.mb_str(wxMBConvUTF16{});
    
    std::u16string u16;
    u16.resize(buf.length() / 2);
    memcpy(u16.data(), buf.data(), buf.length());
    return u16;
}

NS_HWM_END
