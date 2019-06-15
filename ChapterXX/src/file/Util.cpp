#include <regex>
#include <iomanip>

#include "Util.hpp"
#include "../Misc/StringAlgo.hpp"

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

std::vector<std::string> get_lines(std::istream &is)
{
    std::vector<std::string> lines;
    
    while(is) {
        auto line = get_trimmed_line(is);
        if(is) { lines.push_back(line); }
    }
    
    return lines;
}

std::optional<std::string> find_value(std::vector<std::string> const &lines, std::string key)
{
    key = trim(key);
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

std::string base64_encode(char const *data, size_t length)
{
    if(length == 0) { return {}; }
    
    return wxBase64Encode(data, length).ToStdString();
}

std::string base64_encode(std::vector<char> const &data)
{
    return base64_encode(data.data(), data.size());
}

std::vector<char> base64_decode(std::string const &data)
{
    size_t error_pos = -1;
    auto buf = wxBase64Decode(data.data(), data.size(),
                              wxBase64DecodeMode::wxBase64DecodeMode_Strict,
                              &error_pos);
    
    if(error_pos != -1) {
        hwm::dout << "Failed to decode base64 data at: " << error_pos << std::endl;
        return {};
    }
    
    return std::vector<char>((char *)buf.GetData(), (char *)buf.GetData() + buf.GetDataLen());
}

NS_HWM_END
