#include "StrCnv.hpp"
#include <cwchar>
#include <FL/fl_utf8.h>

NS_HWM_BEGIN

std::wstring to_wstr(std::wstring const &str) { return str; }

std::wstring to_wstr(std::string const &str)
{
    std::wstring dest;
    auto const len = fl_utf8towc(str.c_str(), str.size(), &dest[0], dest.size());
    dest.resize(len + 1);
    fl_utf8towc(str.c_str(), str.size(), &dest[0], dest.size());
    return dest;
}

std::wstring to_wstr(std::u16string const &str)
{
    std::string tmp;
    char buf[4];
    for(auto c: str) {
        auto const n = fl_utf8encode(c, buf);
        tmp.append(buf, buf + n);
    }
    
    return to_wstr(tmp);
}

std::string to_utf8(std::wstring const &str)
{
    std::string dest;
    auto const len = fl_utf8fromwc(&dest[0], dest.size(), str.data(), str.size());
    dest.resize(len + 1);
    fl_utf8fromwc(&dest[0], dest.size(), str.data(), str.size());
    return dest;
}

std::u16string to_utf16(std::wstring const &str)
{
    std::u16string tmp;
    std::vector<unsigned short> buf(3);
    for(auto c: str) {
        auto const n = fl_ucs_to_Utf16(c, buf.data(), buf.size());
        tmp.append(buf.begin(), buf.begin() + n);
    }
    
    return tmp;
}

NS_HWM_END
