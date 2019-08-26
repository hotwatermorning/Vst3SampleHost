#pragma once

#include <pluginterfaces/vst/vstspeaker.h>

NS_HWM_BEGIN

inline
String GetSpeakerName(Steinberg::Vst::SpeakerArrangement arr, bool with_speaker_name = true)
{
    return to_wstr(Steinberg::Vst::SpeakerArr::getSpeakerArrangementString(arr, with_speaker_name));
}

NS_HWM_END
