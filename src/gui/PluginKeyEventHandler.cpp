#include "./PluginKeyEventHandler.h"

NS_HWM_BEGIN

#if defined(_MSC_VER)

std::shared_ptr<PluginKeyEventHandler> CreatePluginKeyEventHandler(wxWindow *wnd)
{
    return nullptr;
}

#endif

NS_HWM_END
