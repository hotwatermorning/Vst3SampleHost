#include "PCKeyboardInput.hpp"

#include "../app/App.hpp"

NS_HWM_BEGIN

//! 何らかの原因でKeyUpイベントが取得できなかったときに、キーが押され続けないようにするチェックの間隔
static constexpr UInt32 kTimerIntervalMs = 100;

PCKeyboardInput::PCKeyboardInput()
{
    auto &map = get_key_id_map();
    std::vector<wxAcceleratorEntry> entries;
    for(auto &entry: map) {
        if(entry.first == L'\0') { continue; }
        wxAcceleratorEntry e;
        e.Set(wxACCEL_NORMAL, (int)entry.first, entry.second);
        entries.push_back(e);
    }
    
    acc_table_ = wxAcceleratorTable(entries.size(), entries.data());
    
    for(int i = (int)kID_C; i <= (int)kID_hiEb; ++i) {
        playing_keys_[(KeyID)i] = kInvalidPitch;
    }
    
    timer_.Bind(wxEVT_TIMER, [this](auto &ev) { OnTimer(); });
}

PCKeyboardInput::~PCKeyboardInput()
{}

void PCKeyboardInput::ProcessKeyEvent(wxKeyEvent const &ev)
{
    if(ev.GetModifiers() != 0) {
        return;
    }
    
    bool const is_key_down = ev.GetEventType() == wxEVT_KEY_DOWN;
    bool const is_key_up = ev.GetEventType() == wxEVT_KEY_UP;
    
    if(!is_key_down && !is_key_up) { // unknown event type.
        return;
    }
    
    wchar_t const uc = ev.GetUnicodeKey();
    if(uc == WXK_NONE) {
        return;
    }
    
    auto const id = CharToKeyID(uc);
    if(id == kID_OctDown) {
        if(is_key_down && !octave_down_being_pressed_) {
            TransposeOctaveDown();
        }
        octave_down_being_pressed_ = is_key_down;
        
    } else if(id == kID_OctUp) {
        if(is_key_down && !octave_up_being_pressed_) {
            TransposeOctaveDown();
        }
        
        octave_up_being_pressed_ = is_key_down;
    } else {
        auto const pitch = KeyIDToPitch(id);
        if(pitch == kInvalidPitch) {
            return;
        }
        
        auto app = App::GetInstance();
        
        if(is_key_down) {
            if(pitch == playing_keys_[id]) {
                return;
            }
            
            if(playing_keys_[id] != kInvalidPitch) {
                app->SendNoteOff(playing_keys_[id]);
            }
            
            app->SendNoteOn(pitch);
            playing_keys_[id] = pitch;
        } else {
            if(playing_keys_[id] != kInvalidPitch) {
                app->SendNoteOff(playing_keys_[id]);
            }
            playing_keys_[id] = kInvalidPitch;
        }
    }
    
    timer_.Start(kTimerIntervalMs);
}

Int32 PCKeyboardInput::KeyIDToPitch(KeyID id) const
{
    if((Int32)id < (Int32)kID_C || kID_hiEb < (Int32)id) {
        return kInvalidPitch;
    }
    
    Int32 tmp = base_pitch_ + ((Int32)id - (Int32)kID_C);
    if(tmp < 0 || 128 <= tmp) {
        return kInvalidPitch;
    }
    
    return tmp;
}

void PCKeyboardInput::TransposeOctaveUp()
{
    base_pitch_ = std::min<int>(base_pitch_, 108) + 12;
}

void PCKeyboardInput::TransposeOctaveDown()
{
    base_pitch_ = std::max<int>(base_pitch_, 12) - 12;
}

void PCKeyboardInput::OnTimer()
{
    bool is_pressing_some = false;
    
    std::cout << "PCKeyboardInput Timer" << std::endl;
    for(auto &entry: playing_keys_) {
        if(entry.second == kInvalidPitch) { continue; }
        
        auto c = KeyIDToChar(entry.first);
        auto const narrowed = to_utf8(std::wstring({c}))[0];
        auto const is_pressing = wxGetKeyState((wxKeyCode)narrowed);
        
        auto const is_pressing2 = wxGetKeyState((wxKeyCode)(narrowed + 1));
        
        std::cout << "Is Pressing[" << narrowed << "]: " << is_pressing << std::endl;
        std::cout << "Is Pressing2[" << (narrowed+1) << "]: " << is_pressing2 << std::endl;
        if(is_pressing) {
            is_pressing_some = true;
            continue;
        }
        
        auto app = App::GetInstance();
        std::cout << "send note off: " << entry.second << std::endl;
        app->SendNoteOff(entry.second);
        entry.second = kInvalidPitch;
    }
    
    auto check_octave_key_state = [&](auto &member, KeyID id) {
        auto const narrowed = to_utf8(std::wstring{KeyIDToChar(id)})[0];
        auto const is_pressing = wxGetKeyState((wxKeyCode)narrowed);
        
        if(is_pressing) {
            is_pressing_some = true;
        } else {
            member = false;
        }
    };
    
    check_octave_key_state(octave_up_being_pressed_, kID_OctUp);
    check_octave_key_state(octave_down_being_pressed_, kID_OctDown);

    if(is_pressing_some == false) {
        timer_.Stop();
    }
}

NS_HWM_END
