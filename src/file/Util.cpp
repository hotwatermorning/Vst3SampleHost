#define INSTANCIATE_STRING_CONVERSION_FUNCTIONS

#include <regex>
#include <iomanip>

#include "Util.hpp"
#include "../misc/StringAlgo.hpp"

#include <wx/base64.h>

NS_HWM_BEGIN

Int32 stoi_or(std::string const &str, Int32 default_value)
{
    try {
        return stoi(str);
    } catch(...) {
        return default_value;
    }
}

double stof_or(std::string const &str, double default_value)
{
    try {
        return stoi(str);
    } catch(...) {
        return default_value;
    }
}

std::string get_trimmed_line(std::istream &is)
{
    std::string str;
    std::getline(is, str);
    return trim(str);
}

std::vector<std::string> read_lines(std::istream &is)
{
    std::vector<std::string> lines;
    
    while(is) {
        auto line = get_trimmed_line(is);
        if(is) { lines.push_back(line); }
    }
    
    return lines;
}

std::optional<std::string> find_value(std::vector<std::string> const &lines,
                                      std::string const &key)
{
    assert(key == trim(key));
    std::regex re("^\\s*" + key + "\\s*=\\s*(.*)$");
    std::smatch m;
    for(auto const &line: lines) {
        if(std::regex_match(line, m, re)) {
            std::stringstream ss;
            ss.str(m[1]);
            std::string tmp;
            ss >> std::quoted(tmp);
            return tmp;
        }
    }
    return std::nullopt;
}

std::ostream & operator<<(std::ostream &os, write_line_object const &self)
{
    assert(self.key.find(' ') == std::string::npos &&
           self.key.find('\t') == std::string::npos);
    
    return os << self.key << " = " << std::quoted(self.value);
}

write_line_object write_line(std::string const &key,
                             std::string const &value)
{
    return write_line_object { key, value };
}

std::string base64_encode(char const *data, size_t length)
{
    if(length == 0) { return {}; }
    
    return wxBase64Encode(data, length).ToStdString();
}

std::string base64_encode(std::vector<char> const &data)
{
    return base64_encode(data.data(), data.size());
}

std::optional<std::vector<char>> base64_decode(std::string const &data)
{
    size_t error_pos = -1;
    auto buf = wxBase64Decode(data.data(), data.size(),
                              wxBase64DecodeMode::wxBase64DecodeMode_Strict,
                              &error_pos);
    
    if(error_pos != -1) {
        HWM_DEBUG_LOG(L"Failed to decode base64 data at: " << error_pos);
        return std::nullopt;
    }
    
    return std::vector<char>((char *)buf.GetData(), (char *)buf.GetData() + buf.GetDataLen());
}

template<>
std::string to_s(String const &s)
{
    return to_utf8(s);
}

template<>
std::string to_s(AudioDriverType const &v)
{
    return to_string(v);
}

template<>
std::string to_s(ClassInfo::CID const &v)
{
    return base64_encode(v.data(), v.size());
}

template<>
std::string to_s(std::vector<char> const &v)
{
    return base64_encode(v);
}

template<>
std::string to_s(PluginViewType const &v)
{
    switch(v) {
        case PluginViewType::kGeneric:
            return "generic";
        case PluginViewType::kDedicated:
            return "dedicated";
        default:
            assert(false && "unsupported");
            return "";
    }
}

template<>
std::string to_s(OscillatorType const &v)
{
    switch(v) {
        case OscillatorType::kSine:
            return "sine";
        case OscillatorType::kSaw:
            return "saw";
        case OscillatorType::kSquare:
            return "square";
        case OscillatorType::kTriangle:
            return "triangle";
        default:
            assert(false && "unsupported");
            return "";
    }
}

template<> bool from_s(std::string const &str, String &v)
{
    v = to_wstr(str);
    return true;
}

template<> bool from_s(std::string const &str, AudioDriverType &v)
{
    auto tmp = to_audio_driver_type(str);
    if(tmp) {
        v = *tmp;
        return true;
    } else {
        return false;
    }
}

template<> bool from_s(std::string const &str, std::vector<char> &v)
{
    auto tmp = base64_decode(str);
    if(tmp) {
        v = std::move(*tmp);
        return true;
    } else {
        return false;
    }
}

template<> bool from_s(std::string const &str, ClassInfo::CID &v)
{
    std::vector<char> decoded;
    if(from_s(str, decoded) == false) {
        return false;
    }
    
    if(decoded.size() == v.size()) {
        std::copy_n(decoded.begin(), decoded.size(), v.begin());
        return true;
    } else {
        return false;
    }
}

template<> bool from_s(std::string const &str, PluginViewType &v)
{
    if(str == "generic") {
        v = PluginViewType::kGeneric;
        return true;
    } else if(str == "dedicated") {
        v = PluginViewType::kDedicated;
        return true;
    } else {
        return false;
    }
    
    assert(false && "never reach here");
}

template<> bool from_s(std::string const &str, OscillatorType &v)
{
    if(str == "sine") {
        v = OscillatorType::kSine;
        return true;
    } else if(str == "saw") {
        v = OscillatorType::kSaw;
        return true;
    } else if(str == "square") {
        v = OscillatorType::kSquare;
        return true;
    } else if(str == "triangle") {
        v = OscillatorType::kTriangle;
        return true;
    } else {
        return false;
    }
    
    assert(false && "never reach here");
}

NS_HWM_END
