#include "PCKeyboardInput.hpp"

#include "../App.hpp"

NS_HWM_BEGIN

PCKeyboardInput::PCKeyboardInput()
{
    auto &map = get_key_id_map();
    std::vector<wxAcceleratorEntry> entries;
    for(auto &entry: map) {
        if(entry.first == '\0') { continue; }
        wxAcceleratorEntry e;
        e.Set(wxACCEL_NORMAL, (int)entry.first, entry.second);
        entries.push_back(e);
    }
    
    acc_table_ = wxAcceleratorTable(entries.size(), entries.data());
    
    for(int i = (int)kID_C; i <= (int)kID_hiEb; ++i) {
        playing_keys_[(KeyID)i] = kInvalidPitch;
    }
    
    timer_.Bind(wxEVT_TIMER, [this](auto &ev) { OnTimer(); });
    timer_.Start(50);
}

PCKeyboardInput::~PCKeyboardInput()
{}

void PCKeyboardInput::ApplyTo(wxFrame *frame)
{
    frame->Bind(wxEVT_ACTIVATE, [frame, this](wxActivateEvent &ev) {
        if(ev.GetActive()) {
            frame->SetAcceleratorTable(acc_table_);
        } else {
            frame->SetAcceleratorTable(wxAcceleratorTable{});
        }
    });
    
    frame->Bind(wxEVT_MENU, [this](wxCommandEvent &ev) {
        ev.Skip();
        ev.StopPropagation();
        
        auto const id = (KeyID)ev.GetId();
        if(id == kID_OctDown) {
            TransposeOctaveDown();
        } else if(id == kID_OctUp) {
            TransposeOctaveUp();
        } else {
            auto const pitch = KeyIDToPitch(id);
            if(pitch == kInvalidPitch) {
                return;
            }
            
            if(pitch == playing_keys_[id]) {
                return;
            }
            
            auto app = App::GetInstance();
            
            if(playing_keys_[id] != kInvalidPitch) {
                app->SendNoteOff(playing_keys_[id]);
            }
            
            app->SendNoteOn(pitch);
            playing_keys_[id] = pitch;
        }
    });
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
    for(auto &entry: playing_keys_) {
        if(entry.second == kInvalidPitch) { continue; }
        
        auto c = KeyIDToChar(entry.first);
        auto const narrowed = to_utf8(std::wstring({c}))[0];
        auto const is_key_down = wxGetKeyState((wxKeyCode)narrowed);
        
        if(is_key_down) { continue; }
        
        auto app = App::GetInstance();
        app->SendNoteOff(entry.second);
        entry.second = kInvalidPitch;
    }
}

NS_HWM_END
