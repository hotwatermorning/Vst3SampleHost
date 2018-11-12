#include "Vst3Debug.hpp"

#include <iomanip>
#include <sstream>
#include <pluginterfaces/vst/vstpresetkeys.h>
#include "../../misc/StrCnv.hpp"

using namespace Steinberg;

NS_HWM_BEGIN

void showErrorMsg(tresult result)
{
    if(result == kResultTrue) { return; }
    
    auto to_s = [](auto x) {
        switch(x) {
            case kNoInterface: return "no Interface";
            case kResultFalse: return "result false";
            case kInvalidArgument: return "invalid argument";
            case kNotImplemented: return "not implemented";
            case kInternalError: return "internal error";
            case kNotInitialized: return "not initialized";
            case kOutOfMemory: return "out of memory";
            default: return "unknown error";
        }
    };
    
    hwm::dout << "Error: " << to_s(result) << std::endl;
}


String UnitInfoToString(Vst::UnitInfo const &info)
{
    std::wstringstream ss;
    ss    << info.id
    << L", " << info.name
    << L", " << L"Parent: " << info.parentUnitId
    << L", " << L"Program List ID: " << info.programListId;
    
    return ss.str();
}

String ProgramListInfoToString(Vst::ProgramListInfo const &info)
{
    std::wstringstream ss;
    
    ss    << info.id
    << L", " << L"Program List Name: " << info.name
    << L", " << L"Program Count: " << info.programCount;
    
    return ss.str();
}

void OutputUnitInfo(Vst::IUnitInfo *unit_handler)
{
    assert(unit_handler);
    
    hwm::wdout << "--- Output Unit Info ---" << std::endl;
    
    for(size_t i = 0; i < unit_handler->getUnitCount(); ++i) {
        Vst::UnitInfo unit_info {};
        unit_handler->getUnitInfo(i, unit_info);
        hwm::wdout << L"[" << i << L"] " << UnitInfoToString(unit_info) << std::endl;
    }
    
    hwm::wdout << L"Selected Unit: " << unit_handler->getSelectedUnit() << std::endl;
    
    hwm::wdout << "--- Output Program List Info ---" << std::endl;
    
    for(size_t i = 0; i < unit_handler->getProgramListCount(); ++i) {
        Vst::ProgramListInfo program_list_info {};
        tresult res = unit_handler->getProgramListInfo(i, program_list_info);
        if(res != kResultOk) {
            hwm::wdout << "Getting program list info failed." << std::endl;
            break;
        }
        
        hwm::wdout << L"[" << i << L"] " << ProgramListInfoToString(program_list_info) << std::endl;
        
        for(size_t program_index = 0; program_index < program_list_info.programCount; ++program_index) {
            
            hwm::wdout << L"\t[" << program_index << L"] ";
            
            Vst::String128 name;
            unit_handler->getProgramName(program_list_info.id, program_index, name);
            
            hwm::wdout << to_wstr(name);
            
            Vst::CString attrs[] {
                Vst::PresetAttributes::kPlugInName,
                Vst::PresetAttributes::kPlugInCategory,
                Vst::PresetAttributes::kInstrument,
                Vst::PresetAttributes::kStyle,
                Vst::PresetAttributes::kCharacter,
                Vst::PresetAttributes::kStateType,
                Vst::PresetAttributes::kFilePathStringType };
            
            for(auto attr: attrs) {
                Vst::String128 attr_value = {};
                unit_handler->getProgramInfo(program_list_info.id, program_index, attr, attr_value);
                
                hwm::wdout << L", " << attr << L": " << to_wstr(attr_value);
            }
            
            if(unit_handler->hasProgramPitchNames(program_list_info.id, program_index) == kResultTrue) {
                Vst::String128 pitch_name = {};
                int16 const pitch_center = 0x2000;
                unit_handler->getProgramPitchName(program_list_info.id, program_index, pitch_center, pitch_name);
                
                hwm::wdout << L", " << to_wstr(pitch_name);
            } else {
                hwm::wdout << L", No Pitch Name";
            }
            
            hwm::wdout << std::endl;
        }
    }
}

String BusInfoToString(Vst::BusInfo &bus)
{
    std::wstringstream ss;
    
    ss
    << to_wstr(bus.name)
    << L", " << (bus.mediaType == Vst::MediaTypes::kAudio ? L"Audio" : L"Midi")
    << L", " << (bus.direction == Vst::BusDirections::kInput ? L"Input" : L"Output")
    << L", " << (bus.busType == Vst::BusTypes::kMain ? L"Main Bus" : L"Aux Bus")
    << L", " << L"Channels: " << bus.channelCount
    << L", " << L"Default Active: " << std::boolalpha << ((bus.flags & bus.kDefaultActive) != 0);
    
    return ss.str();
}

// this function returns a string which representing relationship between a bus and units.
String BusUnitInfoToString(int bus_index, Vst::BusInfo &bus, Vst::IUnitInfo *unit_handler, int num_spaces)
{
    auto const spaces = String(num_spaces, L' ');
    
    std::wstringstream ss;
    for(int ch = 0; ch < bus.channelCount; ++ch) {
        Vst::UnitID unit_id;
        // `This method mainly is intended to find out which unit is related to a given MIDI input channel`
        tresult result = unit_handler->getUnitByBus(bus.mediaType, bus.direction, bus_index, ch, unit_id);
        if(result != kResultOk) {
            return spaces + L"Failed to get related unit by bus";
        }
        
        Vst::UnitInfo unit_info;
        auto const num_units = unit_handler->getUnitCount();
        for(int i = 0; i < num_units; ++i) {
            Vst::UnitInfo tmp;
            unit_handler->getUnitInfo(i, tmp);
            if(tmp.id == unit_id) {
                unit_info = tmp;
                break;
            }
            
            assert(i != num_units - 1);
        }
        
        
        ss << spaces << std::setw(2) << ch << "ch => " << unit_id << " (" << to_wstr(unit_info.name) << ")" << std::endl;
    }
    return ss.str();
}

void OutputBusInfoImpl(Vst::IComponent *component,
                       Vst::IUnitInfo *unit_handler,
                       Vst::MediaType media_type,
                       Vst::BusDirection bus_direction)
{
    int const kNumSpaces = 4;
    
    int const num_components = component->getBusCount(media_type, bus_direction);
    if(num_components == 0) {
        hwm::wdout << "No such buses for this component." << std::endl;
        return;
    }
    
    for(int i = 0; i < num_components; ++i) {
        Vst::BusInfo bus_info;
        component->getBusInfo(media_type, bus_direction, i, bus_info);
        
        auto const bus_info_str = BusInfoToString(bus_info);
        auto const bus_unit_info_str
        = (unit_handler && bus_info.channelCount > 0)
        ?   BusUnitInfoToString(i, bus_info, unit_handler, kNumSpaces)
        :   String(kNumSpaces, L' ') + L"No unit info for this bus."
        ;
        
        hwm::wdout << L"[" << i << L"] " << bus_info_str << "\n" << bus_unit_info_str << std::endl;
    }
}

void OutputBusInfo(Vst::IComponent *component,
                   Vst::IEditController *edit_controller,
                   Vst::IUnitInfo *unit_handler)
{
    hwm::dout << "-- output bus info --" << std::endl;
    hwm::dout << "[Audio Input]" << std::endl;
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kAudio, Vst::BusDirections::kInput);
    hwm::dout << "[Audio Output]" << std::endl;
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput);
    hwm::dout << "[Event Input]" << std::endl;
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kEvent, Vst::BusDirections::kInput);
    hwm::dout << "[Event Output]" << std::endl;
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kEvent, Vst::BusDirections::kOutput);
}

NS_HWM_END