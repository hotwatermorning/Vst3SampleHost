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

struct IPluginEditorFrame
:   public SingleInstance<IPluginEditorFrame>
,   public wxFrame
{
    template<class... Args>
    IPluginEditorFrame(Args&&...);
    
    enum class ViewType {
        kGeneric,
        kDedicated,
    };
    
    //! 現在のViewTypeを返す
    virtual
    ViewType GetViewType() const = 0;
    
    //! プラグインのViewTypeを切り替える。
    /*! @return ViewTypeの切り替えに失敗した場合はfalseを返す。
     * 切り替えに成功したり、すでに目的のViewTypeになっている場合はtrueを返す。
     */
    virtual
    bool SetViewType(ViewType type) = 0;
    
    virtual
    void OnResizePlugView() = 0;
};

IPluginEditorFrame * CreatePluginEditorFrame(wxWindow *parent,
                                             Vst3Plugin *target_plugin,
                                             PluginEditorFrameListener *listener);

NS_HWM_END
