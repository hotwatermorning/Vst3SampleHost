#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <objc/message.h>
#import "./PluginKeyEventHandler.h"

typedef void (^EventHandler)(NSEvent *e);

@interface Responder : NSResponder

- (void)dealloc;
- (instancetype)initWithHandler:(EventHandler)hander;
- (void)forwardEvent:(NSEvent *)event;

@property (nonatomic, copy) EventHandler handler;

@end

@implementation Responder

- (void)dealloc
{
    [_handler release];
    [super dealloc];
}

- (instancetype)initWithHandler:(EventHandler)handler
{
    self = [self init];
    _handler = [handler copy];
    return self ?: nil;
}

- (void)forwardEvent:(NSEvent *)theEvent
{
    if (self.handler) {
        self.handler(theEvent);
    }
}

- (void)keyDown:(NSEvent *)theEvent
{
    [self forwardEvent:theEvent];
}

- (void)keyUp:(NSEvent *)theEvent
{
    [self forwardEvent:theEvent];
}

@end

NS_HWM_BEGIN

struct PluginKeyEventHandler
{
    PluginKeyEventHandler(wxWindow *wnd)
    :   wnd_(wnd)
    {
        auto handler = ^(NSEvent *ne) {
            
            if(skip_) { return; }
            
            skip_ = true;
            
            auto view = wnd_->GetHandle();
            auto saved = [view.window firstResponder];
            BOOL const made = [view.window makeFirstResponder:view];
            if(made) {
                if([ne type] == NSEventTypeKeyUp) {
                    [view keyUp:ne];
                } else if([ne type] == NSEventTypeKeyDown) {
                    [view keyDown:ne];
                }
            }
            [view.window makeFirstResponder:saved];
            skip_ = false;
            
            return;
            
            wxKeyEvent we;
            
            if([ne type] == NSEventTypeKeyUp) {
                we.SetEventType(wxEVT_KEY_UP);
            } else if([ne type] == NSEventTypeKeyDown) {
                we.SetEventType(wxEVT_KEY_DOWN);
            }
            
            UInt32 modifiers = [ne modifierFlags];

            we.m_shiftDown = modifiers & NSEventModifierFlagShift;
            we.m_rawControlDown = modifiers & NSEventModifierFlagControl;
            we.m_altDown = modifiers & NSEventModifierFlagOption;
            we.m_controlDown = modifiers & NSEventModifierFlagCommand;
            
            we.m_rawCode = [ne keyCode];
            we.m_rawFlags = modifiers;
            we.SetTimestamp( (int)([ne timestamp] * 1000));
            
            //auto peer = wnd_->GetPeer();
            //peer->keyEvent(event, wnd_->GetHandle(), @selector(keyDown:));
        };
        
        r_ = [[Responder alloc] initWithHandler:handler];
        
        auto view = wnd_->GetHandle();
        NSResponder *next = view.nextResponder;
        [view setNextResponder:r_];
        [r_ setNextResponder:next];
    }
    
    ~PluginKeyEventHandler()
    {
        auto view = wnd_->GetHandle();
        
        NSResponder *cur = view;
        for( ; ; ) {
            auto next = [cur nextResponder];
            if(next == nil) { break; }
            if(next == r_) {
                [cur setNextResponder:r_.nextResponder];
                [r_ setNextResponder:nil];
                break;
            }
            cur = next;
        }
        
        assert(r_.nextResponder == nil);
        [r_ release];
    }
    
    wxWindow *wnd_ = nullptr;
    Responder *r_ = nullptr;
    bool skip_ = false;
};

std::shared_ptr<PluginKeyEventHandler> CreatePluginKeyEventHandler(wxWindow *wnd)
{
    return std::make_shared<PluginKeyEventHandler>(wnd);
}

NS_HWM_END
