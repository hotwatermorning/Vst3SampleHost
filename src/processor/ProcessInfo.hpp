#pragma once

#include "../data_type/MidiDataType.hpp"
#include "../misc/Buffer.hpp"
#include "../misc/ArrayRef.hpp"

NS_HWM_BEGIN

//! 拍子を表す構造体
class Meter
{
public:
    Meter() {}
    Meter(UInt16 numer, UInt16 denom)
    :   numer_(numer)
    ,   denom_(denom)
    {}
    
    UInt16 numer_ = 0;
    UInt16 denom_ = 0;
    
    Tick GetMeasureLength(Tick tpqn) const
    {
        return GetBeatLength(tpqn) * numer_;
    }
    
    Tick GetBeatLength(Tick tpqn) const
    {
        auto const whole_note = tpqn * 4;
        
        //! assert that the resolution of the tpqn is enough to represent the beat length.
        assert(whole_note == (whole_note / denom_) * denom_);
        
        return (whole_note / denom_);
    }
    
    bool operator==(Meter rhs) const
    {
        return (numer_ == rhs.numer_ && denom_ == rhs.denom_);
    }
    
    bool operator!=(Meter rhs) const
    {
        return !(*this == rhs);
    }
};

//! 小節／拍／Tick位置を表す構造体
class MBT
{
public:
    MBT() {}
    MBT(UInt32 measure, UInt16 beat, UInt16 tick)
    :   measure_(measure)
    ,   beat_(beat)
    ,   tick_(tick)
    {}
    
    UInt32 measure_ = 0;
    UInt16 beat_ = 0;
    UInt16 tick_ = 0;
};

struct ProcessInfo
{
    struct TimeInfo
    {
        double sample_rate_ = 44100.0;
        SampleCount sample_pos_ = 0;
        SampleCount sample_length_ = 0;
        double ppq_pos_ = 0;
        bool is_playing_ = false;
        double tempo_ = 120.0;
        Meter meter_ = Meter(4, 4);
    };
    
    struct MidiMessage
    {
        using DataType = MidiDataType::VariantType;
        
        //! フレーム先頭からのオフセット位置
        SampleCount offset_ = 0;
        UInt8 channel_ = 0;
        //! プロジェクト中のPPQ位置
        double ppq_pos_ = 0;

        MidiMessage();
        MidiMessage(SampleCount offset, UInt8 channel, double ppq_pos, DataType data);
        
		template<class To>
		To * As() { return std::get_if<To>(&data_); }

		template<class To>
		To const * As() const { return std::get_if<To>(&data_); }
        
        DataType data_;
    };

    struct IEventBuffer
    {
    protected:
        IEventBuffer() {}
        
    public:
        virtual ~IEventBuffer() {}
        
        virtual
        UInt32 GetCount() const = 0;
        
        virtual
        void AddEvent(MidiMessage const &msg) = 0;
        
        virtual
        MidiMessage const & GetEvent(UInt32 index) const = 0;
        
        virtual
        ArrayRef<MidiMessage const> GetRef() const = 0;
    };
    
    struct IEventBufferList
    {
    protected:
        IEventBufferList() {}
        
    public:
        virtual
        ~IEventBufferList() {}
        
        virtual
        UInt32 GetNumBuffers() const = 0;
        
        virtual
        IEventBuffer * GetBuffer(UInt32 index) = 0;
        
        virtual
        IEventBuffer const * GetBuffer(UInt32 index) const = 0;
    };
    
    TimeInfo                    time_info_;
    BufferRef<float const>      input_audio_buffer_;
    BufferRef<float>            output_audio_buffer_;
    IEventBufferList const *    input_event_buffers_ = nullptr;
    IEventBufferList *          output_event_buffers_ = nullptr;
};

NS_HWM_END
