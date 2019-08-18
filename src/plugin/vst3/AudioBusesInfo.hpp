#pragma once

#include "./Vst3Plugin.hpp"
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>

NS_HWM_BEGIN

class AudioBusesInfo;

class IAudioBusesInfoOwner
{
public:
    using SpeakerList = Vst3Plugin::SpeakerList;
    using IOSpeakerSet = Vst3Plugin::IOSpeakerSet;
    
protected:
    IAudioBusesInfoOwner();
    
public:
    virtual
    ~IAudioBusesInfoOwner();
    
    virtual
    Steinberg::Vst::IComponent * GetComponent() = 0;
    
    virtual
    Steinberg::Vst::IAudioProcessor * GetAudioProcessor() = 0;
    
    void SetInputBuses(AudioBusesInfo *info);
    void SetOutputBuses(AudioBusesInfo *info);
    
    AudioBusesInfo * GetInputBuses();
    AudioBusesInfo * GetOutputBuses();
    
    //! 現在のAudioBusesInfoに設定されているスピーカー設定を返す。
    IOSpeakerSet GetSpeakerArrangements();

    //! 現在VST3プラグインに適用されているスピーカー設定を返す。
    IOSpeakerSet GetAppliedSpeakerArrangements();
    
    //! 指定したIOSpeakerSetに含まれるSpeakerArrangementの状態を、
    //! 入出力それぞれのAudioBusesInfoに設定する。
    /*! @pre speaker_set.input_.size() == GetInputBuses()->GetNumBuses()
     *  @pre speaker_set.output_.size() == GetOutputBuses()->GetNumBuses()
     */
    void SetSpeakerArrangements(IOSpeakerSet const &speaker_set);
 
    //! 現在の入出力それぞれのAudioBusesInfoのスピーカー構成をVST3プラグインに適用し、
    //! AudioBusesInfoのAudioBusBufferを更新する。
    /*! @return 設定が成功したかどうかと、プラグインから修正された（かもしれない）スピーカー構成の情報を含む
     *  std::pairを返す。
     *  成功した場合はpair.firstにSteinberg::kResultTrueをセットし、
     *  pair.secondに最新のスピーカー構成の情報をセットする。
     *  失敗した場合は、pair.firstにエラーコードをセットし、
     *  pair.secondにプラグインから修正された（かもしれない）スピーカー構成の情報をセットする。
     *  プラグインが設定を修正しなかった場合は、GetSpeakerArrangements()と同じものがセットされる。
     */
    std::pair<Steinberg::tresult, IOSpeakerSet> ApplySpeakerArrangements();
    
private:
    AudioBusesInfo *input_ = nullptr;
    AudioBusesInfo *output_ = nullptr;
};

class AudioBusesInfo
{
public:
    using SpeakerList = Vst3Plugin::SpeakerList;
    using BusInfo = Vst3Plugin::BusInfo;
    
    void Initialize(IAudioBusesInfoOwner *owner, Steinberg::Vst::BusDirections dir);
    
    size_t GetNumBuses() const;
    
    BusInfo const & GetBusInfo(UInt32 bus_index) const;
    
    //! すべてのバスのチャンネル数の総計
    //! これは、各バスのSpeakerArrangement状態によって変化する。
    //! バスのアクティブ状態には影響を受けない。
    //!  (つまり、各バスがアクティブかそうでないかに関わらず、すべてのバスのチャンネルが足し合わされる。)
    size_t GetNumChannels() const;
    
    //! すべてのアクティブなバスのチャンネル数の総計
    //! これは、各バスのアクティブ状態やSpeakerArrangement状態によって変化する。
    size_t GetNumActiveChannels() const;
    
    bool IsActive(size_t bus_index) const;
    void SetActive(size_t bus_index, bool state = true);
    
    //! 指定したindex位置のBusのスピーカ構成を設定する。
    /*! ここで設定したスピーカー構成はこの段階ではVST3プラグインに適用されない。
     *  このあとIAudioBusesInfoOwnerのApplySpeakerArrangements()メンバ関数を呼び出すと、
     *  入出力両方のAudioBusesInfoのスピーカー構成がVST3プラグインに適用される。
     *  ApplySpeakerArrangements()が成功すると、
     *  入出力両方のAudioBusesInfoのOnAppliedSpeakerArrangement()が呼び出され、
     *  AudioBusBufferのチャンネル情報が更新される。
     */
    void SetSpeakerArrangement(size_t bus_index, Steinberg::Vst::SpeakerArrangement arr);
    
    //! 現在このAudioBusesInfoに設定されているスピーカー構成を返す。
    SpeakerList GetSpeakerArrangements() const;
    
    //! 現在VST3プラグインに適用されているスピーカー構成を返す。
    SpeakerList GetAppliedSpeakerArrangements() const;

    //! スピーカー構成をまとめてセットする。
    /*! 以下のコードと等価
     *  @code
     *  for(int i = 0; i < speakers.size(); ++i) {
     *      SetSpeakerArrangment(i, speakers[i]);
     *  }
     *  @endcode
     */
    void SetSpeakerArrangements(SpeakerList const &speakers);

    //! バスごとに用意されたAudioBusBufferの情報を返す。
    /*! @return AudioBusBufferの配列を指すポインタ。
     *  配列の要素数とそれぞれのAudioBusBuffersのチャンネル設定は、
     *  GetAppliedSpeaker()で返るSpeakerListの状態に対応している。
     */
    Steinberg::Vst::AudioBusBuffers * GetBusBuffers();
    
private:
    friend IAudioBusesInfoOwner;
    
    IAudioBusesInfoOwner *owner_ = nullptr;
    std::vector<BusInfo> bus_infos_;
    Steinberg::Vst::BusDirections dir_; // determine direction even if the bus_infos_ is empty.
    SpeakerList applied_speakers_;
    
    //! bus_infos_のis_active_状態によらず、定義されているすべてのバスと同じ数だけ用意される。
    std::vector<Steinberg::Vst::AudioBusBuffers> bus_buffers_;
    
    void OnAppliedSpeakerArrangement(SpeakerList const &speakers);
};

NS_HWM_END
