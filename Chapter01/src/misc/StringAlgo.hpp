#pragma once

#include <string>

NS_HWM_BEGIN

//! 文字列両端の空白文字を取り除く
/*! @note 複数行文字列はサポートしていない
 */
std::string trim(std::string const &str);

//! 文字列両端の空白文字を取り除く
/*! @note 複数行文字列はサポートしていない
 */
std::wstring trim(std::wstring const &str);

NS_HWM_END
