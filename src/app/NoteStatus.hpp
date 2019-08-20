#pragma once

#include <cassert>
#include <array>
#include <atomic>
#include <tuple>

NS_HWM_BEGIN

struct alignas(4) NoteStatus {
    enum Type : char {
        kNull,
        kNoteOn,
        kNoteOff,
    };
    
    static NoteStatus CreateNoteOn(UInt8 velocity)
    {
        assert(velocity != 0);
        return NoteStatus { kNoteOn, velocity, 0 };
    }
    
    static NoteStatus CreateNoteOff(UInt8 off_velocity)
    {
        return NoteStatus { kNoteOff, off_velocity, 0 };
    }
    
    static NoteStatus CreateNull()
    {
        return NoteStatus {};
    }
    
    bool is_null() const { return type_ == kNull; }
    bool is_note_on() const { return type_ == kNoteOn; }
    bool is_note_off() const { return type_ == kNoteOff; }
    UInt8 get_velocity() const { return velocity_; }
    
    bool operator==(NoteStatus const &rhs) const
    {
        auto to_tuple = [](auto const &self) {
            return std::tie(self.type_, self.velocity_);
        };
        
        return to_tuple(*this) == to_tuple(rhs);
    }
    
    bool operator!=(NoteStatus const &rhs) const
    {
        return !(*this == rhs);
    }
    
    Type type_ = Type::kNull;
    UInt8 velocity_ = 0;
    UInt16 dummy_padding_ = 0;
};

using KeyboardStatus = std::array<std::atomic<NoteStatus>, 128>;

NS_HWM_END
