#pragma once

#include "./GlobalLogger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

NS_HWM_BEGIN

//! Initialize the global logger with the default logging levels (Error, Warn, Info, Debug).
void InitializeDefaultGlobalLogger();

bool IsEnabledErrorCheckAssertionForLoggingMacros();
void EnableErrorCheckAssertionForLoggingMacros(bool enable);

template<class Stream>
struct LoggingStreamWrapper
{
    LoggingStreamWrapper(Stream &s)
    :   s_(&s)
    {}
    
    Stream & base() { return static_cast<Stream &>(*s_); }
    
public:
    static constexpr bool false_value_ = false;
    
private:
    Stream *s_;
};

template<class Stream, class T>
LoggingStreamWrapper<Stream> &
operator<<(LoggingStreamWrapper<Stream> &os, T arg)
{
    os.base() << arg;
    return os;
}

template<class Stream>
LoggingStreamWrapper<Stream> &
operator<<(LoggingStreamWrapper<Stream> &os, std::wostream & (*arg)(std::wostream &))
{
    (*arg)(os.base());
    return os;
}

//! マルチバイト文字列は受け付けない
template<class Stream>
LoggingStreamWrapper<Stream> & operator<<(LoggingStreamWrapper<Stream> &os, std::string const &rhs)
{
    static_assert(LoggingStreamWrapper<Stream>::false_value_, "multi byte string is not allowed");
    return os;
}

//! マルチバイト文字列は受け付けない
template<class Stream>
LoggingStreamWrapper<Stream> & operator<<(LoggingStreamWrapper<Stream> &os, char const *rhs)
{
    static_assert(LoggingStreamWrapper<Stream>::false_value_, "multi byte string is not allowed");
    return os;
}

// logging macro.
#define HWM_LOG(level, ...) \
do { \
    if(auto global_logger_ref ## __LINE__ = GetGlobalLogger()) { \
        auto global_logger_ref_output_error = global_logger_ref ## __LINE__ ->OutputLog(level, [&] { \
            std::wstringstream ss; \
            LoggingStreamWrapper<std::wstringstream> wss(ss); \
            wss << __VA_ARGS__; \
            return ss.str(); \
        }); \
        if(IsEnabledErrorCheckAssertionForLoggingMacros()) { \
            assert(global_logger_ref_output_error.has_error() == false); \
        } \
    } \
} while(0) \
/* */

// dedicated logging macros for the default logging levels.

//! 処理の中断を伴うようなエラーが発生したことを表すログ
#define HWM_ERROR_LOG(...) HWM_LOG(L"Error", __VA_ARGS__)
//! 処理の中断を伴わないようなエラーが発生したことを表すログ
#define HWM_WARN_LOG(...) HWM_LOG(L"Warn", __VA_ARGS__)
//! 通常のログ。処理が問題なく進行していることを表す
#define HWM_INFO_LOG(...) HWM_LOG(L"Info", __VA_ARGS__)
//! デバッグ用の詳細なログ
#define HWM_DEBUG_LOG(...) HWM_LOG(L"Debug", __VA_ARGS__)

NS_HWM_END
