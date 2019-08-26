#pragma once

#include <pluginterfaces/base/ipluginbase.h>

NS_HWM_BEGIN

class FactoryInfo
{
public:
    bool discardable                () const;
    bool license_check                () const;
    bool component_non_discardable    () const;
    bool unicode                    () const;
    
    String    vendor    () const;
    String    url        () const;
    String    email    () const;
    
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
