#pragma once

#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include "../device/DeviceType.hpp"

NS_HWM_BEGIN

Int32 stoi_or(std::string const &str, Int32 default_value);

double stof_or(std::string const &str, double default_value);

std::string get_trimmed_line(std::istream &is);

std::vector<std::string> get_lines(std::istream &is);

std::optional<std::string> find_value(std::vector<std::string> const &lines, std::string key);

std::string base64_encode(std::vector<char> const &data);
std::string base64_encode(char const *data, size_t length);

std::vector<char> base64_decode(std::string const &data);

template<class T>
std::string to_s(T const &s) {
    std::stringstream ss;
    ss << s;
    return ss.str();
}

template<>
inline
std::string to_s<String>(String const &s) { return to_utf8(s); }

template<>
inline
std::string to_s<AudioDriverType>(AudioDriverType const &s) { return to_string(s); }

template<>
inline
std::string to_s<std::vector<char>>(std::vector<char> const &s) { return base64_encode(s); }

NS_HWM_END
