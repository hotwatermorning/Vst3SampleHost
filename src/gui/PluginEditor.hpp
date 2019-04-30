#pragma once

#include <functional>
#include "../plugin/vst3/Vst3Plugin.hpp"

NS_HWM_BEGIN

struct PluginEditorFrameListener
{
protected:
    PluginEditorFrameListener();
    
public:
    virtual ~PluginEditorFrameListener();
    
    virtual
    void OnDestroyFromPluginEditorFrame() = 0;
};

wxFrame * CreatePluginEditorFrame(wxWindow *parent,
                                  Vst3Plugin *target_plugin,
                                  PluginEditorFrameListener *listener);

NS_HWM_END
