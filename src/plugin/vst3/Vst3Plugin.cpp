#include "Vst3Plugin.hpp"
#include "Vst3PluginImpl.hpp"
#include "Vst3HostContext.hpp"
#include "VstMAUtils.hpp"
#include "FactoryInfo.hpp"

#include <cassert>
#include <memory>

NS_HWM_BEGIN

using namespace Steinberg;

bool Vst3Plugin::IOSpeakerSet::operator==(IOSpeakerSet const &rhs) const
{
    return input_ == rhs.input_ && output_ == rhs.output_;
}

bool Vst3Plugin::IOSpeakerSet::operator!=(IOSpeakerSet const &rhs) const
{
    return !(*this == rhs);
}

Vst3Plugin::Vst3Plugin(IPluginFactory *factory, ClassInfo const &class_info)
{
    HWM_INFO_LOG(L"Construct a Vst3Plugin [" << class_info.GetName() + L"]");
    assert(factory);
    
    auto host_context = std::make_unique<Vst3Plugin::HostContext>(kAppName);

    pimpl_ = std::make_unique<Impl>(factory,
                                    class_info,
                                    host_context->unknownCast());
    
    host_context_ = std::move(host_context);
    host_context_->SetVst3Plugin(this);
}

Vst3Plugin::~Vst3Plugin()
{
    HWM_INFO_LOG(L"Destruct a Vst3Plugin: [" << GetComponentInfo().GetName() << L"]");
    vpdls_.Invoke([this](auto *li) {
        li->OnBeforeDestruction(this);
    });
    
    assert(IsEditorOpened() == false);
    
    pimpl_.reset();

    host_context_.reset();
}

FactoryInfo const & Vst3Plugin::GetFactoryInfo() const
{
    return pimpl_->GetFactoryInfo();
}

ClassInfo const & Vst3Plugin::GetComponentInfo() const
{
    return pimpl_->GetComponentInfo();
}

String Vst3Plugin::GetPluginName() const
{
    return pimpl_->GetPluginName();
}

size_t Vst3Plugin::GetNumAudioInputs() const
{
    return pimpl_->GetAudioBusesInfo(Vst::BusDirections::kInput).GetNumActiveChannels();
}

size_t Vst3Plugin::GetNumAudioOutputs() const
{
    return pimpl_->GetAudioBusesInfo(Vst::BusDirections::kOutput).GetNumActiveChannels();
}

UInt32  Vst3Plugin::GetNumParams() const
{
    return pimpl_->GetParameterInfoList().size();
}

Vst3Plugin::ParameterInfo const & Vst3Plugin::GetParameterInfoByIndex(UInt32 index) const
{
    return pimpl_->GetParameterInfoList().GetItemByIndex(index);
}
std::optional<Vst3Plugin::ParameterInfo>
    Vst3Plugin::FindParameterInfoByID(ParamID id) const
{
    return pimpl_->GetParameterInfoList().FindItemByID(id);
}

UInt32  Vst3Plugin::GetNumUnitInfo() const
{
    return pimpl_->GetUnitInfoList().size();
}

Vst3Plugin::UnitInfo const & Vst3Plugin::GetUnitInfoByIndex(UInt32 index) const
{
    return pimpl_->GetUnitInfoList().GetItemByIndex(index);
}

Vst3Plugin::UnitInfo const & Vst3Plugin::GetUnitInfoByID(UnitID id) const
{
    return pimpl_->GetUnitInfoList().GetItemByID(id);
}

UInt32  Vst3Plugin::GetNumBuses(MediaTypes media, BusDirections dir) const
{
    if(media == MediaTypes::kAudio) {
        return pimpl_->GetAudioBusesInfo(dir).GetNumBuses();
    } else {
        return pimpl_->GetMidiBusesInfo(dir).GetNumBuses();
    }
}

Vst3Plugin::BusInfo const & Vst3Plugin::GetBusInfoByIndex(MediaTypes media, BusDirections dir, UInt32 index) const
{
    if(media == MediaTypes::kAudio) {
        return pimpl_->GetAudioBusesInfo(dir).GetBusInfo(index);
    } else {
        return pimpl_->GetMidiBusesInfo(dir).GetBusInfo(index);
    }
}

Vst3Plugin::ParamValue Vst3Plugin::GetParameterValueByIndex(UInt32 index) const
{
    return pimpl_->GetParameterValueByIndex(index);
}

Vst3Plugin::ParamValue Vst3Plugin::GetParameterValueByID(ParamID id) const
{
    return pimpl_->GetParameterValueByID(id);
}

void Vst3Plugin::SetParameterValueByIndex(UInt32 index, ParamValue value)
{
    pimpl_->SetParameterValueByIndex(index, value);
}

void Vst3Plugin::SetParameterValueByID(ParamID id, ParamValue value)
{
    pimpl_->SetParameterValueByID(id, value);
}

String Vst3Plugin::ValueToStringByIndex(UInt32 index, ParamValue value)
{
    return pimpl_->ValueToStringByIndex(index, value);
}

Vst::ParamValue Vst3Plugin::StringToValueTByIndex(UInt32 index, String string)
{
    return pimpl_->StringToValueTByIndex(index, string);
}

String Vst3Plugin::ValueToStringByID(ParamID id, ParamValue value)
{
    return pimpl_->ValueToStringByID(id, value);
}

Vst::ParamValue Vst3Plugin::StringToValueByID(ParamID id, String string)
{
    return pimpl_->StringToValueByID(id, string);
}

bool Vst3Plugin::IsBusActive(MediaTypes media, BusDirections dir, UInt32 index) const
{
    return GetBusInfoByIndex(media, dir, index).is_active_;
}

void Vst3Plugin::SetBusActive(MediaTypes media, BusDirections dir, UInt32 index, bool state)
{
    if(media == MediaTypes::kAudio) {
        pimpl_->GetAudioBusesInfo(dir).SetActive(index, state);
    } else {
        pimpl_->GetMidiBusesInfo(dir).SetActive(index, state);
    }
}

UInt32 Vst3Plugin::GetNumActiveBuses(MediaTypes media, BusDirections dir) const
{
    assert(media == MediaTypes::kEvent);
    return pimpl_->GetMidiBusesInfo(dir).GetNumActiveBuses();
}

Vst3Plugin::SpeakerArrangement Vst3Plugin::GetSpeakerArrangementForBus(BusDirections dir, UInt32 index) const
{
    return pimpl_->GetAudioBusesInfo(dir).GetBusInfo(index).speaker_;
}

Vst3Plugin::IOSpeakerSet Vst3Plugin::GetSpeakerArrangements() const
{
    return pimpl_->audio_buses_info_owner_->GetSpeakerArrangements();
}

Vst3Plugin::IOSpeakerSet Vst3Plugin::GetAppliedSpeakerArrangements() const
{
    return pimpl_->audio_buses_info_owner_->GetAppliedSpeakerArrangements();
}

void Vst3Plugin::SetSpeakerArrangements(IOSpeakerSet const &speakers)
{
    return pimpl_->audio_buses_info_owner_->SetSpeakerArrangements(speakers);
}

void Vst3Plugin::SetSpeakerArrangement(BusDirections dir, UInt32 index, SpeakerArrangement arr)
{
    return pimpl_->GetAudioBusesInfo(dir).SetSpeakerArrangement(index, arr);
}

std::pair<Steinberg::tresult, Vst3Plugin::IOSpeakerSet>
Vst3Plugin::ApplySpeakerArrangement()
{
    return pimpl_->audio_buses_info_owner_->ApplySpeakerArrangements();
}

void Vst3Plugin::Resume()
{
    pimpl_->Resume();
}

void Vst3Plugin::Suspend()
{
    //CloseEditor();
    pimpl_->Suspend();
}

bool Vst3Plugin::IsResumed() const
{
    return pimpl_->IsResumed();
}

void Vst3Plugin::SetBlockSize(int block_size)
{
    assert(!IsResumed());
    pimpl_->SetBlockSize(block_size);
}

void Vst3Plugin::SetSamplingRate(int sampling_rate)
{
    assert(!IsResumed());
    pimpl_->SetSamplingRate(sampling_rate);
}

bool Vst3Plugin::HasEditor() const
{
    return pimpl_->HasEditor();
}

bool Vst3Plugin::OpenEditor(WindowHandle parent, IPlugFrameListener *listener)
{
    //! currently multiple plug view is not supported yet.
    CloseEditor();
    
    assert(listener);
    assert(host_context_->plug_frame_listener_ == nullptr);
    
    host_context_->plug_frame_listener_ = listener;
    return pimpl_->OpenEditor(parent, host_context_.get());
}

void Vst3Plugin::CloseEditor()
{
    if(pimpl_->plug_view_) {
        pimpl_->plug_view_->removed();
        pimpl_->plug_view_.reset();
    }
    
    host_context_->plug_frame_listener_ = nullptr;
}

bool Vst3Plugin::IsEditorOpened() const
{
    return !!pimpl_->plug_view_;
}

bool Vst3Plugin::CanChangeEditorSize() const
{
    if(!IsEditorOpened()) { return false; }
    
    return pimpl_->plug_view_->canResize() == kResultTrue;
}

ViewRect Vst3Plugin::GetEditorSize() const
{
    ViewRect rect { 0, 0, 0, 0 };
    
    if(IsEditorOpened()) {
        auto const ret = pimpl_->plug_view_->getSize(&rect);
        if(ret != kResultTrue) {
            HWM_DEBUG_LOG(L"SetEditorSize failed: " << ret);
        }
    }
    
    return rect;
}

void Vst3Plugin::SetEditorSize(ViewRect const &rc)
{
    if(!pimpl_->plug_view_) { return; }
    auto tmp = rc;
    auto const ret = pimpl_->plug_view_->onSize(&tmp);
    if(ret != kResultTrue) {
        HWM_DEBUG_LOG(L"SetEditorSize failed: " << ret);
    }
}

bool Vst3Plugin::GetEffectiveEditorSize(Steinberg::ViewRect &rc)
{
    if(!IsEditorOpened()) { return false; }
    
    return pimpl_->plug_view_->checkSizeConstraint(&rc) == kResultTrue;
}

UInt32 Vst3Plugin::GetProgramIndex(UnitID id) const
{
    return pimpl_->GetProgramIndex(id);
}

void Vst3Plugin::SetProgramIndex(UInt32 index, UnitID id)
{
    pimpl_->SetProgramIndex(index, id);
}

void Vst3Plugin::EnqueueParameterChange(Vst::ParamID id, Vst::ParamValue value)
{
    pimpl_->PushBackParameterChange(id, value);
}

void Vst3Plugin::RestartComponent(Steinberg::int32 flags)
{
    pimpl_->RestartComponent(flags);
}

void Vst3Plugin::Process(ProcessInfo &pi)
{
    pimpl_->Process(pi);
}

std::optional<Vst3Plugin::DumpData> Vst3Plugin::SaveData() const
{
    return pimpl_->SaveData();
}

void Vst3Plugin::LoadData(DumpData const &dump)
{
    pimpl_->LoadData(dump);
}

Vst3Plugin::Vst3PluginListenerService & Vst3Plugin::GetVst3PluginListenerService()
{
    return host_context_->vpls_;
}

Vst3Plugin::Vst3PluginDestructionListenerService & Vst3Plugin::GetVst3PluginDestructionListenerService()
{
    return vpdls_;
}

NS_HWM_END
