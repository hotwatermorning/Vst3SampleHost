#pragma once

#include <vector>
#include <unordered_map>

#include <pluginterfaces/vst/ivstcomponent.h>

#include "./Vst3Plugin.hpp"

NS_HWM_BEGIN

class IMidiBusesInfoOwner
{
protected:
    IMidiBusesInfoOwner() {}
    
public:
    virtual
    ~IMidiBusesInfoOwner() {}
    
    virtual
    Steinberg::Vst::IComponent * GetComponent() = 0;
};

class MidiBusesInfo
{
public:
    using BusInfo = Vst3Plugin::BusInfo;
    
    void Initialize(IMidiBusesInfoOwner *owner,
                    Steinberg::Vst::BusDirections dir);
    
    size_t GetNumBuses() const;
    
    BusInfo const & GetBusInfo(UInt32 bus_index) const;
    
    bool IsActive(size_t bus_index) const;
    void SetActive(size_t bus_index, bool state = true);
    
    UInt32 GetNumActiveBuses() const;
    UInt32 GetBusIndexFromActiveBusIndex(UInt32 active_bus_index) const;
    UInt32 GetActiveBusIndexFromBusIndex(UInt32 bus_index) const;
    
private:
    IMidiBusesInfoOwner *owner_ = nullptr;
    std::vector<BusInfo> bus_infos_;
    Steinberg::Vst::BusDirections dir_;
    std::unordered_map<UInt32, UInt32> bus_index_to_active_bus_index_;
    std::unordered_map<UInt32, UInt32> active_bus_index_to_bus_index_;
    
    void SetupActiveBusTable();
};

NS_HWM_END
