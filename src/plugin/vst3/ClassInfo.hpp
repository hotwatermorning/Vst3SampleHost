#pragma once

#include <array>
#include <pluginterfaces/base/ipluginbase.h>

NS_HWM_BEGIN

class ClassInfo2Data
{
public:
    ClassInfo2Data(Steinberg::PClassInfo2 const &info);
    ClassInfo2Data(Steinberg::PClassInfoW const &info);
    
    String const &    GetSubCategories() const { return sub_categories_; }
    String const &    GetVendor() const { return vendor_; }
    String const &    GetVersion() const { return version_; }
    String const &    GetSDKVersion() const { return sdk_version_; }
    
    bool HasSubCategory(String elem) const;
    
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
    
    CID const &    GetCID() const { return cid_; }
    String const &    GetName() const { return name_; }
    String const &    GetCategory() const { return category_; }
    Steinberg::int32  GetCardinality() const { return cardinality_; }
    
    bool HasClassInfo2() const { return static_cast<bool>(classinfo2_data_); }
    ClassInfo2Data const & GetClassInfo2() const { return *classinfo2_data_; }
    
    bool IsEffect() const;
    bool IsInstrument() const;
    
private:
    CID cid_ = {{}};
    String        name_;
    String        category_;
    Steinberg::int32    cardinality_ = -1;
    std::optional<ClassInfo2Data> classinfo2_data_;
};


NS_HWM_END
