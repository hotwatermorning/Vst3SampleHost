#include "ClassInfo.hpp"

#include <algorithm>
#include "../../misc/StrCnv.hpp"

NS_HWM_BEGIN

ClassInfo2Data::ClassInfo2Data(Steinberg::PClassInfo2 const &info)
:    sub_categories_(hwm::to_wstr(info.subCategories))
,    vendor_(hwm::to_wstr(info.vendor))
,    version_(hwm::to_wstr(info.version))
,    sdk_version_(hwm::to_wstr(info.sdkVersion))
{
}

ClassInfo2Data::ClassInfo2Data(Steinberg::PClassInfoW const &info)
:    sub_categories_(hwm::to_wstr(info.subCategories))
,    vendor_(hwm::to_wstr(info.vendor))
,    version_(hwm::to_wstr(info.version))
,    sdk_version_(hwm::to_wstr(info.sdkVersion))
{
}

bool ClassInfo2Data::HasSubCategory(String elem) const
{
    wxArrayString list = wxSplit(sub_categories_, L'|');
    return std::any_of(list.begin(), list.end(),
                       [elem](wxString const &x) {
                           bool const is_case_sensitive = false;
                           return x.IsSameAs(elem, is_case_sensitive);
                       }
                       );
}

ClassInfo::ClassInfo()
{}

ClassInfo::ClassInfo(Steinberg::PClassInfo const &info)
:    cid_()
,    name_(hwm::to_wstr(info.name))
,    category_(hwm::to_wstr(info.category))
,    cardinality_(info.cardinality)
{
    std::copy(info.cid, info.cid + kCIDLength, cid_.begin());
}

ClassInfo::ClassInfo(Steinberg::PClassInfo2 const &info)
:    cid_()
,    name_(hwm::to_wstr(info.name))
,    category_(hwm::to_wstr(info.category))
,    cardinality_(info.cardinality)
,    classinfo2_data_(info)
{
    std::copy(info.cid, info.cid + kCIDLength, cid_.begin());
}

ClassInfo::ClassInfo(Steinberg::PClassInfoW const &info)
:    cid_()
,    name_(hwm::to_wstr(info.name))
,    category_(hwm::to_wstr(info.category))
,    cardinality_(info.cardinality)
,    classinfo2_data_(info)
{
    std::copy(info.cid, info.cid + kCIDLength, cid_.begin());
}

bool ClassInfo::IsEffect() const
{
    return HasClassInfo2() && GetClassInfo2().HasSubCategory(L"fx");
}

bool ClassInfo::IsInstrument() const
{
    return HasClassInfo2() && GetClassInfo2().HasSubCategory(L"instrument");
}

NS_HWM_END
