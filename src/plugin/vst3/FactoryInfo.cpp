#include "FactoryInfo.hpp"
#include "Vst3Utils.hpp"

NS_HWM_BEGIN

using namespace Steinberg;

FactoryInfo::FactoryInfo(PFactoryInfo const &info)
:    vendor_(RawArrayToWideString(info.vendor))
,    url_(RawArrayToWideString(info.url))
,    email_(RawArrayToWideString(info.email))
,    flags_(info.flags)
{}

bool FactoryInfo::IsDiscardable() const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kClassesDiscardable) != 0;
}

bool FactoryInfo::IsLicenseCheck() const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kLicenseCheck) != 0;
}

bool FactoryInfo::IsComponentNonDiscardable() const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kComponentNonDiscardable) != 0;
}

bool FactoryInfo::IsUnicode() const
{
    return (flags_ & Steinberg::PFactoryInfo::FactoryFlags::kUnicode) != 0;
}

String    FactoryInfo::GetVendor() const
{
    return vendor_;
}

String    FactoryInfo::GetURL() const
{
    return url_;
}

String    FactoryInfo::GetEmail() const
{
    return email_;
}

NS_HWM_END
