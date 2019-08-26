#include "FactoryInfo.hpp"
#include "../../misc/StrCnv.hpp"

NS_HWM_BEGIN

using namespace Steinberg;

FactoryInfo::FactoryInfo(PFactoryInfo const &info)
:    vendor_(hwm::to_wstr(info.vendor))
,    url_(hwm::to_wstr(info.url))
,    email_(hwm::to_wstr(info.email))
,    flags_(info.flags)
{}

bool FactoryInfo::discardable                () const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kClassesDiscardable) != 0;
}

bool FactoryInfo::license_check                () const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kLicenseCheck) != 0;
}

bool FactoryInfo::component_non_discardable    () const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kComponentNonDiscardable) != 0;
}

bool FactoryInfo::unicode                    () const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kUnicode) != 0;
}

String    FactoryInfo::vendor    () const
{
    return vendor_;
}

String    FactoryInfo::url        () const
{
    return url_;
}

String    FactoryInfo::email    () const
{
    return email_;
}

NS_HWM_END
