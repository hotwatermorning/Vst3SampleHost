#pragma once

#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include "../device/DeviceType.hpp"
#include "../plugin/vst3/Vst3PluginFactory.hpp"
#include "../gui/PluginViewType.hpp"

NS_HWM_BEGIN

Int32 stoi_or(std::string const &str, Int32 default_value);

double stof_or(std::string const &str, double default_value);

std::string get_trimmed_line(std::istream &is);

std::vector<std::string> read_lines(std::istream &is);

std::optional<std::string> find_value(std::vector<std::string> const &lines, std::string const &key);

struct write_line_object {
    std::string const key;
    std::string const value;
    
    friend
    std::ostream & operator<<(std::ostream &os, write_line_object const &self);
};

write_line_object write_line(std::string const &key, std::string const &value);

std::string base64_encode(std::vector<char> const &data);
std::string base64_encode(char const *data, size_t length);

std::optional<std::vector<char>> base64_decode(std::string const &data);

template<class T>
std::string to_s(T const &v) {
    std::stringstream ss;
    ss << v;
    return ss.str();
}

template<class T>
std::string to_s(std::optional<T> const &v) {
    if(v) {
        return to_s(*v);
    } else {
        return "";
    }
}

template<class T>
bool from_s(std::string const &str, T &v)
{
    std::istringstream ss(str);
    ss >> v;
    return !!ss;
}

template<class T>
bool from_s(std::string const &str, std::optional<T> &v)
{
    T tmp;
    if(from_s(str, tmp)) {
        v = tmp;
        return true;
    } else {
        return false;
    }
}

#if !defined(INSTANCIATE_STRING_CONVERSION_FUNCTIONS)

extern template std::string to_s(String const &s);
extern template std::string to_s(AudioDriverType const &v);
extern template std::string to_s(ClassInfo::CID const &v);
extern template std::string to_s(std::vector<char> const &v);
extern template std::string to_s(PluginViewType const &v);

extern template bool from_s(std::string const &str, String &s);
extern template bool from_s(std::string const &str, AudioDriverType &v);
extern template bool from_s(std::string const &str, std::vector<char> &v);
extern template bool from_s(std::string const &str, ClassInfo::CID &v);
extern template bool from_s(std::string const &str, PluginViewType &v);

#endif

NS_HWM_END
