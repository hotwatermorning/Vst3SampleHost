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
        kID_Device_Preferences,
        kID_File_Load,
        kID_File_Save,
    };
    
    template<class... Args>
    IMainFrame(Args&&...);
};

IMainFrame * CreateMainFrame();

NS_HWM_END
