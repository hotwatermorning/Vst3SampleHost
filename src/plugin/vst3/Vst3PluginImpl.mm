#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

NS_HWM_BEGIN

void * GetWindowRef(void *view)
{
    auto wnd = static_cast<NSView *>(view).window;
    return wnd.windowRef;
}

NS_HWM_END
