#include "BusUtil.hpp"

NS_HWM_BEGIN

using namespace Steinberg;

std::vector<Vst3Plugin::BusInfo> CreateBusInfoList(Vst::IComponent *comp,
                                                   Vst::MediaTypes media,
                                                   Vst::BusDirections dir)
{
    std::vector<Vst3Plugin::BusInfo> bus_infos;
    size_t const num_buses = comp->getBusCount(media, dir);
    bus_infos.resize(num_buses);
    
    tresult ret = kResultTrue;
    for(size_t i = 0; i < num_buses; ++i) {
        Vst::BusInfo vbi;
        ret = comp->getBusInfo(media, dir, i, vbi);
        if(ret != kResultTrue) { throw std::runtime_error("Failed to get BusInfo"); }
        
        Vst3Plugin::BusInfo bi;
        bi.bus_type_ = static_cast<Vst::BusTypes>(vbi.busType);
        bi.channel_count_ = vbi.channelCount;
        bi.direction_ = static_cast<Vst::BusDirections>(vbi.direction);
        bi.is_default_active_ = (vbi.flags & Vst::BusInfo::kDefaultActive) != 0;
        bi.media_type_ = static_cast<Vst::MediaTypes>(vbi.mediaType);
        bi.name_ = to_wstr(vbi.name);
        bi.is_active_ = bi.is_default_active_;
        
        bus_infos[i] = bi;
    }
    
    return bus_infos;
}

NS_HWM_END
