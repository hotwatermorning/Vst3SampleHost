#pragma once

//! @file
/*! 文字列型を変換する関数を定義するファイル
 */

#include <string>

NS_HWM_BEGIN

//! 引数に渡した文字列をワイド文字列に変換する。
/*! この関数は他の文字列型との整合性のためだけに用意してある。元の文字列の複製を作って返す。
 */
std::wstring to_wstr(std::wstring const &str);
//! 引数に渡した文字列をワイド文字列に変換する。
/*! この関数は他の文字列型との整合性のためだけに用意してある。元の文字列の複製を作って返す。
 */
std::wstring to_wstr(wchar_t const *first, wchar_t const *last);

//! 引数に渡した文字列をUTF8文字列とみなしてワイド文字列に変換する
std::wstring to_wstr(std::string const &str);
//! 引数に渡した文字列をUTF8文字列とみなしてワイド文字列に変換する
std::wstring to_wstr(char const *first, char const *last);

//! 引数に渡したUTF16文字列をワイド文字列に変換する
std::wstring to_wstr(std::u16string const &str);
//! 引数に渡したUTF16文字列をワイド文字列に変換する
std::wstring to_wstr(char16_t const *first, char16_t const *last);

//! 引数に渡したワイド文字列をUTF8文字列なstd::stringとして返す。
std::string to_utf8(std::wstring const &str);
//! 引数に渡したワイド文字列をUTF8文字列なstd::stringとして返す。
std::string to_utf8(wchar_t const *first, wchar_t const *last);

//! 引数に渡したワイド文字列をUTF16文字列として返す。
std::u16string to_utf16(std::wstring const &str);
//! 引数に渡したワイド文字列をUTF16文字列として返す。
std::u16string to_utf16(wchar_t const *first, wchar_t const *last);

NS_HWM_END
