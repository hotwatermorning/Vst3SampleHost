#include "catch2/catch.hpp"

#include "../misc/StrCnv.hpp"

using namespace hwm;

TEST_CASE("string conversion test", "[strcnv]")
{
    std::wstring const wstr = L"こんにちは";
    std::string const utf8 = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF";
    std::u16string const utf16 = u"こんにちは";
    
    CHECK(utf8 == to_utf8(wstr));
    CHECK(utf8 == to_utf8(wstr.c_str(), wstr.c_str() + wstr.size()));
    CHECK(utf16 == to_utf16(wstr));
    CHECK(utf16 == to_utf16(wstr.c_str(), wstr.c_str() + wstr.size()));
    
    CHECK(wstr == to_wstr(utf8));
    CHECK(wstr == to_wstr(utf8.c_str(), utf8.c_str() + utf8.size()));
    CHECK(wstr == to_wstr(utf16));
    CHECK(wstr == to_wstr(utf16.c_str(), utf16.c_str() + utf16.size()));
}
