#pragma once

#include <functional>
#include "../plugin/vst3/Vst3Plugin.hpp"
#include "./PluginViewType.hpp"

NS_HWM_BEGIN

struct IPluginEditorFrameListener
{
protected:
    IPluginEditorFrameListener();
    
public:
    virtual ~IPluginEditorFrameListener();
    
    virtual
    void OnDestroyFromPluginEditorFrame() = 0;
};

struct IPluginEditorFrame
:   public SingleInstance<IPluginEditorFrame>
,   public wxFrame
{
    template<class... Args>
    IPluginEditorFrame(Args&&...);
    
    //! 現在のViewTypeを返す
    virtual
    PluginViewType GetViewType() const = 0;
    
    //! プラグインのViewTypeを切り替える。
    /*! @return ViewTypeの切り替えに失敗した場合はfalseを返す。
     * 切り替えに成功したり、すでに目的のViewTypeになっている場合はtrueを返す。
     */
    virtual
    bool SetViewType(PluginViewType type) = 0;
    
    virtual
    void OnResizePlugView() = 0;
};

IPluginEditorFrame * CreatePluginEditorFrame(wxWindow *parent,
                                             Vst3Plugin *target_plugin,
                                             IPluginEditorFrameListener *listener);

NS_HWM_END
