#pragma once

#include <pluginterfaces/base/ipluginbase.h>

NS_HWM_BEGIN

class ClassInfo2Data
{
public:
    ClassInfo2Data(Steinberg::PClassInfo2 const &info);
    ClassInfo2Data(Steinberg::PClassInfoW const &info);
    
    String const &    sub_categories() const { return sub_categories_; }
    String const &    vendor() const { return vendor_; }
    String const &    version() const { return version_; }
    String const &    sdk_version() const { return sdk_version_; }
    
    bool has_sub_category(String elem) const;
    
private:
    String    sub_categories_;
    String    vendor_;
    String    version_;
    String    sdk_version_;
};

class ClassInfo
{
public:
    static constexpr UInt32 kCIDLength = 16;
    using CID = std::array<Steinberg::int8, kCIDLength>;
    
    ClassInfo();
    ClassInfo(Steinberg::PClassInfo const &info);
    ClassInfo(Steinberg::PClassInfo2 const &info);
    ClassInfo(Steinberg::PClassInfoW const &info);
    
    CID const &    cid() const { return cid_; }
    String const &    name() const { return name_; }
    String const &    category() const { return category_; }
    Steinberg::int32        cardinality() const { return cardinality_; }
    
    bool has_classinfo2() const { return static_cast<bool>(classinfo2_data_); }
    ClassInfo2Data const &
    classinfo2() const { return *classinfo2_data_; }
    
    bool is_fx() const;
    bool is_instrument() const;
    
private:
    CID cid_ = {{}};
    String        name_;
    String        category_;
    Steinberg::int32    cardinality_ = -1;
    std::optional<ClassInfo2Data> classinfo2_data_;
};


NS_HWM_END
