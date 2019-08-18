#pragma once

#include "Vst3Plugin.hpp"

NS_HWM_BEGIN

std::vector<Vst3Plugin::BusInfo> CreateBusInfoList(Steinberg::Vst::IComponent *comp,
                                                   Steinberg::Vst::MediaTypes media,
                                                   Steinberg::Vst::BusDirections dir);

NS_HWM_END
