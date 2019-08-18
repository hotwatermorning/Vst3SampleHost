#include "./StringAlgo.hpp"
#include "./StrCnv.hpp"
#include <regex>

NS_HWM_BEGIN

std::string trim(std::string const &str)
{
    return to_utf8(trim(to_wstr(str)));
}

std::wstring trim(std::wstring const &str)
{
    std::wregex re(LR"(^\s+|\s+$)");
    return std::regex_replace(str, re, L"");
}

NS_HWM_END
