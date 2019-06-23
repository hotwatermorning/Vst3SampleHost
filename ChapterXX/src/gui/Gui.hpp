#pragma once

#include "../misc/SingleInstance.hpp"

NS_HWM_BEGIN

class IMainFrame
:   public wxFrame
,   public SingleInstance<IMainFrame>
{
public:
    enum {
        kID_Playback_EnableAudioInputs = wxID_HIGHEST + 1,
        kID_Playback_Waveform_Sine,
        kID_Playback_Waveform_Saw,
        kID_Playback_Waveform_Square,
        kID_Playback_Waveform_Triangle,
        kID_Device_Preferences,
        kID_File_Load,
        kID_File_Save,
        kID_View_PluginEditor,
    };
    
    template<class... Args>
    IMainFrame(Args&&...);
};

IMainFrame * CreateMainFrame();

NS_HWM_END
