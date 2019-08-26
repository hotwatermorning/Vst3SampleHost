#pragma once

#include <pluginterfaces/vst/vstspeaker.h>
#include <type_traits>
#include "../../misc/StrCnv.hpp"

NS_HWM_BEGIN

inline
String GetSpeakerName(Steinberg::Vst::SpeakerArrangement arr, bool with_speaker_name = true)
{
    return to_wstr(Steinberg::Vst::SpeakerArr::getSpeakerArrangementString(arr, with_speaker_name));
}

template<class RawArray>
String RawArrayToWideString(RawArray const &arr)
{
    static_assert(std::is_array<RawArray>::value, "arr must be a reference of raw array");
    auto const found = std::find(std::begin(arr), std::end(arr), 0);
    
    return to_wstr(std::begin(arr), found);
}

NS_HWM_END
