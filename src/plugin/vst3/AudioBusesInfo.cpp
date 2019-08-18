#include "AudioBusesInfo.hpp"
#include "BusUtil.hpp"

#include <numeric>

NS_HWM_BEGIN

using namespace Steinberg;

IAudioBusesInfoOwner::IAudioBusesInfoOwner()
{}

IAudioBusesInfoOwner::~IAudioBusesInfoOwner()
{}

void IAudioBusesInfoOwner::SetInputBuses(AudioBusesInfo *info)
{
    assert(output_ == nullptr);
    input_ = info;
}

void IAudioBusesInfoOwner::SetOutputBuses(AudioBusesInfo *info)
{
    assert(output_ == nullptr);
    output_ = info;
}

AudioBusesInfo * IAudioBusesInfoOwner::GetInputBuses()
{
    return input_;
}

AudioBusesInfo * IAudioBusesInfoOwner::GetOutputBuses()
{
    return output_;
}

IAudioBusesInfoOwner::IOSpeakerSet
IAudioBusesInfoOwner::GetSpeakerArrangements()
{
    assert(input_ && output_);
    
    IOSpeakerSet speakers;
    speakers.input_ = input_->GetSpeakerArrangements();
    speakers.output_ = output_->GetSpeakerArrangements();
    
    return speakers;
}

IAudioBusesInfoOwner::IOSpeakerSet
IAudioBusesInfoOwner::GetAppliedSpeakerArrangements()
{
    assert(input_ && output_);
    
    IOSpeakerSet speakers;
    speakers.input_ = input_->GetAppliedSpeakerArrangements();
    speakers.output_ = output_->GetAppliedSpeakerArrangements();
    
    return speakers;
}

void IAudioBusesInfoOwner::SetSpeakerArrangements(IOSpeakerSet const &speakers)
{
    input_->SetSpeakerArrangements(speakers.input_);
    output_->SetSpeakerArrangements(speakers.output_);
}

std::pair<Steinberg::tresult, IAudioBusesInfoOwner::IOSpeakerSet>
IAudioBusesInfoOwner::ApplySpeakerArrangements()
{
    std::pair<Steinberg::tresult, IAudioBusesInfoOwner::IOSpeakerSet> ret;
    
    auto speakers = GetSpeakerArrangements();
    
    auto const result =
    GetAudioProcessor()->setBusArrangements(speakers.input_.data(),
                                            speakers.input_.size(),
                                            speakers.output_.data(),
                                            speakers.output_.size());
    
    if(result == Steinberg::kResultTrue) {
        input_->OnAppliedSpeakerArrangement(speakers.input_);
        output_->OnAppliedSpeakerArrangement(speakers.output_);
    }
    
    return std::make_pair(result, std::move(speakers));
}

void AudioBusesInfo::Initialize(IAudioBusesInfoOwner *owner, Vst::BusDirections dir)
{
    owner_ = owner;
    dir_ = dir;
    
    if(dir_ == Vst::kInput) {
        owner_->SetInputBuses(this);
    } else {
        owner_->SetOutputBuses(this);
    }
    
    auto const media = Vst::MediaTypes::kAudio;
    auto bus_infos = CreateBusInfoList(owner_->GetComponent(), media, dir);
    
    for(size_t i = 0; i < bus_infos.size(); ++i) {
        Vst::SpeakerArrangement arr;
        auto ret = owner_->GetAudioProcessor()->getBusArrangement(dir, i, arr);
        if(ret != kResultTrue) { throw std::runtime_error("Failed to get SpeakerArrangement"); }
        bus_infos[i].speaker_ = arr;
    }
    
    bus_infos_ = std::move(bus_infos);
    
    OnAppliedSpeakerArrangement(GetSpeakerArrangements());
}

size_t AudioBusesInfo::GetNumBuses() const
{
    return bus_infos_.size();
}

AudioBusesInfo::BusInfo const &
AudioBusesInfo::GetBusInfo(UInt32 bus_index) const
{
    assert(bus_index < GetNumBuses());
    return bus_infos_[bus_index];
}

size_t AudioBusesInfo::GetNumChannels() const
{
    return std::accumulate(bus_infos_.begin(),
                           bus_infos_.end(),
                           0,
                           [](size_t sum, auto const &info) {
                               return sum + info.channel_count_;
                           });
}

size_t AudioBusesInfo::GetNumActiveChannels() const
{
    return std::accumulate(bus_infos_.begin(),
                           bus_infos_.end(),
                           0,
                           [](size_t sum, auto const &info) {
                               return sum + (info.is_active_ ? info.channel_count_ : 0);
                           });
}

bool AudioBusesInfo::IsActive(size_t bus_index) const
{
    return GetBusInfo(bus_index).is_active_;
}

void AudioBusesInfo::SetActive(size_t bus_index, bool state)
{
    assert(bus_index < GetNumBuses());
    
    auto *comp = owner_->GetComponent();
    auto const result = comp->activateBus(Vst::MediaTypes::kAudio, dir_, bus_index, state);
    if(result != kResultTrue) {
        throw std::runtime_error("Failed to activate a bus");
    }
    
    bus_infos_[bus_index].is_active_ = state;
}

//! @return true if this speaker arrangement is accepted to the plugin successfully,
//! false otherwise.
void AudioBusesInfo::SetSpeakerArrangement(size_t bus_index, Vst::SpeakerArrangement arr)
{
    assert(bus_index < GetNumBuses());
    
    bus_infos_[bus_index].speaker_ = arr;
    bus_infos_[bus_index].channel_count_ = Vst::SpeakerArr::getChannelCount(arr);
}

AudioBusesInfo::SpeakerList AudioBusesInfo::GetSpeakerArrangements() const
{
    std::vector<Vst::SpeakerArrangement> arrs;
    size_t const num = GetNumBuses();
    for(size_t i = 0; i < num; ++i) {
        arrs.push_back(GetBusInfo(i).speaker_);
    }
    
    return arrs;
}

AudioBusesInfo::SpeakerList AudioBusesInfo::GetAppliedSpeakerArrangements() const
{
    return applied_speakers_;
}

void AudioBusesInfo::SetSpeakerArrangements(SpeakerList const &speakers)
{
    for(Int32 i = 0; i < speakers.size(); ++i) {
        SetSpeakerArrangement(i, speakers[i]);
    }
}

Vst::AudioBusBuffers * AudioBusesInfo::GetBusBuffers()
{
    return bus_buffers_.data();
}

void AudioBusesInfo::OnAppliedSpeakerArrangement(SpeakerList const &speakers)
{
    applied_speakers_ = std::move(speakers);
    
    bus_buffers_.clear();
    
    for(auto const &s: applied_speakers_) {
        Vst::AudioBusBuffers tmp = {};
        tmp.numChannels = Vst::SpeakerArr::getChannelCount(s);
        bus_buffers_.push_back(tmp);
    }
}

NS_HWM_END
