#include "Vst3Debug.hpp"

#include <iomanip>
#include <sstream>
#include <pluginterfaces/vst/vstpresetkeys.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include "../../misc/StrCnv.hpp"
#include "Vst3Utils.hpp"
#include "VstMAUtils.hpp"

using namespace Steinberg;

NS_HWM_BEGIN

std::string tresult_to_string(tresult result)
{
    switch(result) {
        case kResultOk: return "successful";
        case kNoInterface: return "no Interface";
        case kResultFalse: return "result false";
        case kInvalidArgument: return "invalid argument";
        case kNotImplemented: return "not implemented";
        case kInternalError: return "internal error";
        case kNotInitialized: return "not initialized";
        case kOutOfMemory: return "out of memory";
        default: return "unknown error";
    }
}

std::wstring tresult_to_wstring(tresult result)
{
    return to_wstr(tresult_to_string(result));
}

Steinberg::tresult ShowError(Steinberg::tresult result, String context)
{
    if(result != kResultOk) {
        HWM_WARN_LOG(L"VST3 Error: " <<
                     wxString::Format(L"Failed(%ls): %ls",
                                      tresult_to_wstring(result), context
                                      ).ToStdWstring());
    }
    
    return result;
}

void OutputParameterInfo(Vst::IEditController *edit_controller)
{
    HWM_DEBUG_LOG(L"--- Output Parameter Info ---");
    
    UInt32 const num = edit_controller->getParameterCount();
    
    for(UInt32 i = 0; i < num; ++i) {
        Vst::ParameterInfo info;
        edit_controller->getParameterInfo(i, info);
        
        auto has_flag = [info](auto flag) { return (info.flags & flag) != 0 ? L"true" : L"false"; };
        HWM_DEBUG_LOG(
           wxString::Format(L"[%4d]: {"
                            L"ID:%d, Title:%ls, ShortTitle:%ls, Units:%ls, "
                            L"StepCount:%d, DefaultValue:%0.6lf, UnitID:%d, "
                            L"CanAutomate:%ls, IsReadOnly:%ls, IsWrapAround:%ls, "
                            L"IsList:%ls, IsProgramChange:%ls, IsBypass:%ls "
                            L"}",
                            i,
                            (Int32)info.id, to_wstr(info.title), to_wstr(info.shortTitle), to_wstr(info.units),
                            (Int32)info.stepCount, (double)info.defaultNormalizedValue, (Int32)info.unitId,
                            has_flag(info.kCanAutomate), has_flag(info.kIsReadOnly), has_flag(info.kIsWrapAround),
                            has_flag(info.kIsList), has_flag(info.kIsProgramChange), has_flag(info.kIsBypass)
                            ).ToStdWstring()
                      );
    }
}

String UnitInfoToString(Vst::UnitInfo const &info)
{
    return
    wxString::Format(L"ID:%d, Name:%ls, Parent:%d, ProgramListID:%d",
                     (Int32)info.id, to_wstr(info.name), (Int32)info.parentUnitId, (Int32)info.programListId
                     ).ToStdWstring();
}

String ProgramListInfoToString(Vst::ProgramListInfo const &info)
{
    return
    wxString::Format(L"ID:%d, Name:%ls, ProgramCount:%d",
                     (Int32)info.id, to_wstr(info.name), (Int32)info.programCount
                     ).ToStdWstring();
}

void OutputUnitInfo(Vst::IUnitInfo *unit_handler)
{
    assert(unit_handler);
    
    HWM_DEBUG_LOG(L"--- Output Unit Info ---");
    
    for(size_t i = 0; i < unit_handler->getUnitCount(); ++i) {
        Vst::UnitInfo unit_info {};
        unit_handler->getUnitInfo(i, unit_info);
        HWM_DEBUG_LOG(L"[" << i << L"] " << UnitInfoToString(unit_info));
    }
    
    HWM_DEBUG_LOG(L"Selected Unit: " << unit_handler->getSelectedUnit());
    
    HWM_DEBUG_LOG(L"--- Output Program List Info ---");
    
    std::wstringstream ss;
    for(size_t i = 0; i < unit_handler->getProgramListCount(); ++i) {
        Vst::ProgramListInfo program_list_info {};
        tresult res = unit_handler->getProgramListInfo(i, program_list_info);
        if(res != kResultOk) {
            HWM_WARN_LOG(L"Getting program list info failed.");
            break;
        }
        
        ss << wxString::Format(L"[%d] %ls", (Int32)i, ProgramListInfoToString(program_list_info)).ToStdWstring();
        
        for(size_t program_index = 0; program_index < program_list_info.programCount; ++program_index) {
            
            ss << wxString::Format(L"\t[%d] ", (Int32)program_index).ToStdWstring();
            
            Vst::String128 name;
            unit_handler->getProgramName(program_list_info.id, program_index, name);
            
            ss << to_wstr(name);
            
            Vst::CString attrs[] {
                Vst::PresetAttributes::kPlugInName,
                Vst::PresetAttributes::kPlugInCategory,
                Vst::PresetAttributes::kInstrument,
                Vst::PresetAttributes::kStyle,
                Vst::PresetAttributes::kCharacter,
                Vst::PresetAttributes::kStateType,
                Vst::PresetAttributes::kFilePathStringType,
                Vst::PresetAttributes::kFileName
            };
            
            for(auto attr: attrs) {
                Vst::String128 attr_value = {};
                unit_handler->getProgramInfo(program_list_info.id, program_index, attr, attr_value);
                
                ss << L", " << to_wstr(attr) << ":[" << to_wstr(attr_value) << L"]";
            }
            
            if(unit_handler->hasProgramPitchNames(program_list_info.id, program_index) == kResultTrue) {
                Vst::String128 pitch_name = {};
                int16 const pitch_center = 0x2000;
                unit_handler->getProgramPitchName(program_list_info.id, program_index, pitch_center, pitch_name);
                
                ss << L", " << to_wstr(pitch_name);
            } else {
                ss << L", No Pitch Name";
            }
            
            ss << std::endl;
        }
        
        HWM_DEBUG_LOG(ss.str());
    }
}

String BusInfoToString(Vst::BusInfo &bus, Vst::SpeakerArrangement speaker)
{
    return
    wxString::Format(L"{ Name:%ls, MediaType:%ls, Direction:%ls"
                     "BusType:%ls, Channels:%d, DefaultActive:%ls, Speaker:[%ls] }",
                     to_wstr(bus.name),
                     (bus.mediaType == Vst::MediaTypes::kAudio ? L"Audio" : L"Midi"),
                     (bus.direction == Vst::BusDirections::kInput ? L"Input" : L"Output"),
                     (bus.busType == Vst::BusTypes::kMain ? L"Main Bus" : L"Aux Bus"),
                     (Int32)bus.channelCount,
                     ((bus.flags & bus.kDefaultActive) != 0) ? L"true" : L"false",
                     GetSpeakerName(speaker)
                     ).ToStdWstring();
}

// this function returns a string which representing relationship between a bus and units.
String BusUnitInfoToString(int bus_index, Vst::BusInfo const &bus, Vst::IUnitInfo *unit_handler, int num_spaces)
{
    auto const spaces = String(num_spaces, L' ');
    
    auto get_unit_info_of_channel = [&](int channel) -> String {
        Vst::UnitID unit_id;
        // `This method mainly is intended to find out which unit is related to a given MIDI input channel`
        tresult result = unit_handler->getUnitByBus(bus.mediaType, bus.direction, bus_index, channel, unit_id);
        if(result == kResultOk) {
            // ok
        } else if(result == kResultFalse) {
            return spaces + L"No related unit info for this bus channel.";
        } else {
            return spaces + L"Failed to get related unit info for this bus channel: " + to_wstr(tresult_to_string(result));
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
            
            if(i != num_units - 1) {
                return spaces + L"No related unit info for this bus channel.";
            }
        }

        return spaces + wxString::Format(L"Channel:%02d => Unit:%ls",
                                         (Int32)channel, to_wstr(unit_info.name)
                                         ).ToStdWstring();
    };

    String str;
    for(int ch = 0; ch < bus.channelCount; ++ch) {
        str += get_unit_info_of_channel(ch);
        if(ch != bus.channelCount-1) { str += L"\n"; }
    }
    return str;
}

void OutputBusInfoImpl(Vst::IComponent *component,
                       Vst::IUnitInfo *unit_handler,
                       Vst::MediaTypes media_type,
                       Vst::BusDirections bus_direction)
{
    int const kNumSpaces = 4;
    
    int const num_components = component->getBusCount(media_type, bus_direction);
    if(num_components == 0) {
        HWM_DEBUG_LOG(L"No such buses for this component.");
        return;
    }
    
    for(int i = 0; i < num_components; ++i) {
        Vst::BusInfo bus_info;
        component->getBusInfo(media_type, bus_direction, i, bus_info);
        auto ap = std::move(queryInterface<Vst::IAudioProcessor>(component).right());

        Vst::SpeakerArrangement speaker;
        ap->getBusArrangement(bus_direction, i, speaker);
        
        auto const bus_info_str = BusInfoToString(bus_info, speaker);
        auto const bus_unit_info_str
        = (unit_handler && bus_info.channelCount > 0)
        ?   BusUnitInfoToString(i, bus_info, unit_handler, kNumSpaces)
        :   String(kNumSpaces, L' ') + L"No unit info for this bus."
        ;
        
        HWM_DEBUG_LOG(wxString::Format(L"[%d] %ls\n%ls", (Int32)i, bus_info_str, bus_unit_info_str).ToStdWstring());
    }
}

void OutputBusInfo(Vst::IComponent *component,
                   Vst::IEditController *edit_controller,
                   Vst::IUnitInfo *unit_handler)
{
    HWM_DEBUG_LOG(L"-- output bus info --");
    HWM_DEBUG_LOG(L"[Audio Input]");
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kAudio, Vst::BusDirections::kInput);
    HWM_DEBUG_LOG(L"[Audio Output]");
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kAudio, Vst::BusDirections::kOutput);
    HWM_DEBUG_LOG(L"[Event Input]");
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kEvent, Vst::BusDirections::kInput);
    HWM_DEBUG_LOG(L"[Event Output]");
    OutputBusInfoImpl(component, unit_handler, Vst::MediaTypes::kEvent, Vst::BusDirections::kOutput);
}

NS_HWM_END
