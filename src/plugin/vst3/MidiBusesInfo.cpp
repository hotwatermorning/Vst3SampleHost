#include "MidiBusesInfo.hpp"

#include "BusUtil.hpp"

NS_HWM_BEGIN

using namespace Steinberg;

void MidiBusesInfo::Initialize(IMidiBusesInfoOwner *owner, Vst::BusDirections dir)
{
    dir_ = dir;
    owner_ = owner;
    assert(owner_);
    
    auto const media = Vst::MediaTypes::kEvent;
    
    auto comp = owner_->GetComponent();
    auto bus_infos = CreateBusInfoList(comp, media, dir);
    
    bus_infos_ = std::move(bus_infos);
}

size_t MidiBusesInfo::GetNumBuses() const
{
    return bus_infos_.size();
}

Vst3Plugin::BusInfo const &
MidiBusesInfo::GetBusInfo(UInt32 bus_index) const
{
    assert(bus_index < GetNumBuses());
    return bus_infos_[bus_index];
}

bool MidiBusesInfo::IsActive(size_t bus_index) const
{
    assert(bus_index < GetNumBuses());
    return bus_infos_[bus_index].is_active_;
}

void MidiBusesInfo::SetActive(size_t bus_index, bool state)
{
    assert(bus_index < GetNumBuses());
    
    auto comp = owner_->GetComponent();
    auto const result = comp->activateBus(Vst::MediaTypes::kEvent, dir_, bus_index, state);
    if(result != kResultTrue) {
        throw std::runtime_error("Failed to activate a bus");
    }
    
    bus_infos_[bus_index].is_active_ = state;
    SetupActiveBusTable();
}

UInt32 MidiBusesInfo::GetNumActiveBuses() const
{
    return active_bus_index_to_bus_index_.size();
}

UInt32 MidiBusesInfo::GetBusIndexFromActiveBusIndex(UInt32 active_bus_index) const
{
    auto found = active_bus_index_to_bus_index_.find(active_bus_index);
    if(found == active_bus_index_to_bus_index_.end()) { return -1; }
    
    return found->second;
}

UInt32 MidiBusesInfo::GetActiveBusIndexFromBusIndex(UInt32 bus_index) const
{
    auto found = bus_index_to_active_bus_index_.find(bus_index);
    if(found == bus_index_to_active_bus_index_.end()) { return -1; }
    
    return found->second;
}

void MidiBusesInfo::SetupActiveBusTable()
{
    bus_index_to_active_bus_index_.clear();
    active_bus_index_to_bus_index_.clear();
    
    for(int i = 0; i < bus_infos_.size(); ++i) {
        if(bus_infos_[i].is_active_) {
            auto bus_index = i;
            auto active_bus_index = active_bus_index_to_bus_index_.size();
            
            bus_index_to_active_bus_index_.emplace(bus_index, active_bus_index);
            active_bus_index_to_bus_index_.emplace(active_bus_index, bus_index);
        }
    }
}

NS_HWM_END
