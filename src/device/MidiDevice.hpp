#pragma once

#include "./DeviceType.hpp"

NS_HWM_BEGIN

struct MidiDeviceInfo
{
    DeviceIOType io_type_;
    String name_id_;
    
    bool operator==(MidiDeviceInfo const &rhs) const
    {
        return  (this->name_id_ == rhs.name_id_)
        &&      (this->io_type_ == rhs.io_type_);
    }
    
    bool operator!=(MidiDeviceInfo const &rhs) const
    {
        return !(*this == rhs);
    }
};

class IMidiDevice
{
protected:
    IMidiDevice() {}
    
public:
    virtual
    ~IMidiDevice() {}
    
    virtual
    MidiDeviceInfo const & GetDeviceInfo() const = 0;
};

NS_HWM_END
