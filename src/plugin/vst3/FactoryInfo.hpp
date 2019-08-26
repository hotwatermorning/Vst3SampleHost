#pragma once

#include <pluginterfaces/base/ipluginbase.h>

NS_HWM_BEGIN

class FactoryInfo
{
public:
    bool IsDiscardable() const;
    bool IsLicenseCheck() const;
    bool IsComponentNonDiscardable() const;
    bool IsUnicode() const;
    
    String    GetVendor() const;
    String    GetURL() const;
    String    GetEmail() const;
    
public:
    FactoryInfo() {}
    FactoryInfo(Steinberg::PFactoryInfo const &info);
    
private:
    String vendor_;
    String url_;
    String email_;
    Steinberg::int32 flags_;
};


NS_HWM_END
