#include "catch2/catch.hpp"

#include "../misc/StringAlgo.hpp"

using namespace hwm;

TEST_CASE("string algorithm trim test", "[string_algo_trim]")
{
    std::string s1 = "   a bc";
    std::string s2 = "a bc    ";
    std::string s3 = "  \t a bc  \r \f  ";

    REQUIRE(trim(s1) == "a bc");
    REQUIRE(trim(s2) == "a bc");
    REQUIRE(trim(s3) == "a bc");
    
    std::wstring ws1 = L"   a bc";
    std::wstring ws2 = L"a bc    ";
    std::wstring ws3 = L"  \t a bc  \r \f  ";
    
    REQUIRE(trim(ws1) == L"a bc");
    REQUIRE(trim(ws2) == L"a bc");
    REQUIRE(trim(ws3) == L"a bc");
}
