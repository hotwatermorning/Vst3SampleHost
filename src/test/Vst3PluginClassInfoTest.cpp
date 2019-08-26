#include "catch2/catch.hpp"
#include <algorithm>

#include "../plugin/vst3/ClassInfo.hpp"
#include "../misc/StrCnv.hpp"

using namespace hwm;

TEST_CASE("Vst3PluginClassInfoTest", "[vst3]")
{
    using namespace Steinberg;
    
    PClassInfo2 pc;
    
    static auto write_string = [](auto &dest_arr, std::string const &str)
    {
        auto const dest_size = std::size(dest_arr);
        assert(dest_size > 0);
        
        if(str.size() > dest_size) { throw std::runtime_error("invalid string length."); }
        std::copy(str.begin(), str.end(), dest_arr);
        if(str.size() < dest_size) {
            dest_arr[str.size()] = '\0';
        }
    };
    
    write_string(pc.category, "test category");
    write_string(pc.name, "test name");
    write_string(pc.sdkVersion, "test sdk version");
    write_string(pc.vendor, "test vendor");
    write_string(pc.version, "test version");

    static auto find_sub_category = [&pc](std::string const &sub_categories,
                                   std::string const &find_value)
    {
        write_string(pc.subCategories, sub_categories);
        ClassInfo ci(pc);
        assert(ci.HasClassInfo2());
        return ci.GetClassInfo2().HasSubCategory(to_wstr(find_value));
    };
    
    CHECK(find_sub_category("fx", "fx") == true);
    CHECK(find_sub_category("instrument", "instrument") == true);
    CHECK(find_sub_category("abc|Fx|def", "fx") == true);
    CHECK(find_sub_category("abc|Fx|def|instrument", "Instrument") ==  true);
    CHECK(find_sub_category("abc|instrumen", "Instrument") ==  false);

    {
        FUID fuid;
        fuid.fromRegistryString("{c200e360-38c5-11ce-ae62-08002b2b79ef}");
        fuid.toTUID(pc.cid);
        write_string(pc.subCategories, "test_sub_categories");
        ClassInfo ci(pc);
        
        CHECK(ci.GetCategory() == L"test category");
        CHECK(std::equal(ci.GetCID().begin(), ci.GetCID().end(), pc.cid));
        CHECK(ci.GetName() == L"test name");
        CHECK(ci.GetClassInfo2().GetSDKVersion() == L"test sdk version");
        CHECK(ci.GetClassInfo2().GetSubCategories() == L"test_sub_categories");
        CHECK(ci.GetClassInfo2().GetVendor() == L"test vendor");
        CHECK(ci.GetClassInfo2().GetVersion() == L"test version");
    }
}
