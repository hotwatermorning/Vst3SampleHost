#pragma once

#include <array>
#include <memory>

#include <functional>

#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstunits.h>

#include "../../processor/ProcessInfo.hpp"
#include "../../misc/ListenerService.hpp"
#include "../../misc/Buffer.hpp"
#include "../../misc/ArrayRef.hpp"
#include "./IdentifiedValueList.hpp"
#include "./Vst3PluginFactory.hpp"

namespace Steinberg {
    class IPluginFactory;
}

NS_HWM_BEGIN

//! プラグイン側の状態変更を通知するリスナークラス
struct IVst3PluginListener
:   IListenerBase
{
protected:
    IVst3PluginListener()
    {}
    
public:
    //! プラグインエディターでの編集操作を開始したことを通知するコールバック
    /*! @param plugin 対象のプラグイン
     *  @param id 対象のパラメータのID
     */
    virtual void OnBeginEdit(Vst3Plugin *plugin, Steinberg::Vst::ParamID id)
    {}
    
    //! パラメータの編集されている値を通知するコールバック
    /*! @param plugin 対象のプラグイン
     *  @param id 対象のパラメータのID
     */
    virtual void OnPerformEdit(Vst3Plugin *plugin, Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized)
    {}
    
    //! パラメータの編集操作が終了したことを通知するコールバック
    /*! @param plugin 対象のプラグイン
     *  @param id 対象のパラメータのID
     */
    virtual void OnEndEdit(Vst3Plugin *plugin, Steinberg::Vst::ParamID id)
    {}
    
    //! コンポーネントの再起動要求を通知するコールバック
    virtual void OnRestartComponent(Vst3Plugin *plugin, Steinberg::int32 flags)
    {}
    
    //! プラグインがDirty状態が変更されたことを Host に通知するコールバック
    virtual void OnSetDirty(Vst3Plugin *plugin, Steinberg::TBool state)
    {}
    
    //! エディターのオープンをリクエストするコールバック
    virtual void OnRequestOpenEditor(Vst3Plugin *plugin, String name)
    {}
    
    //! 複数パラメータの同時編集操作の開始を通知するコールバック
    virtual void OnStartGroupEdit(Vst3Plugin *plugin)
    {}
    
    //! 複数パラメータの同時編集操作の終了を通知するコールバック
    virtual void OnFinishGroupEdit(Vst3Plugin *plugin)
    {}
    
    //! プラグイン側でアクティブな Unit が変更されたことを通知するコールバック
    virtual void OnNotifyUnitSelection(Vst3Plugin *plugin, Steinberg::Vst::UnitID unitId)
    {}
    
    //! プラグイン側でアクティブな Program が変更されたことを通知するコールバック
    virtual void OnNotifyProgramListChange(Vst3Plugin *plugin,
                                           Steinberg::Vst::ProgramListID listId,
                                           Steinberg::int32 programIndex)
    {}
    
    //! Unit と Bus の紐付けが変更されたことを通知するコールバック
    virtual void OnNotifyUnitByBusChange(Vst3Plugin *plugin)
    {}
};

//! Vst3Plugin が破棄されることを通知するリスナークラス
struct IVst3PluginDestructionListener
:   IListenerBase
{
protected:
    IVst3PluginDestructionListener()
    {}
    
public:
    virtual void OnBeforeDestruction(Vst3Plugin *plugin)
    {}
};

//! VST3のプラグインを表すクラス
/*!
	Vst3PluginFactoryから作成可能
	Vst3PluginFactoryによってロードされたモジュール（.vst3ファイル）がアンロードされてしまうため、
	作成したVst3Pluginが破棄されるまで、Vst3PluginFactoryを破棄してはならない。
*/
class Vst3Plugin
{
public:
	class Impl;
    class HostContext;
    
    using ParamID = Steinberg::Vst::ParamID;
    using ParamValue = Steinberg::Vst::ParamValue;
    using ProgramListID = Steinberg::Vst::ProgramListID;
    using UnitID = Steinberg::Vst::UnitID;
    using SpeakerArrangement = Steinberg::Vst::SpeakerArrangement;
    using MediaTypes = Steinberg::Vst::MediaTypes;
    using BusDirections = Steinberg::Vst::BusDirections;
    using BusTypes = Steinberg::Vst::BusTypes;
    using SpeakerList = std::vector<SpeakerArrangement>;
    
    //! 入出力すべての AudioBus のスピーカー構成を表すクラス
    struct IOSpeakerSet {
        SpeakerList input_;
        SpeakerList output_;
        
        bool operator==(IOSpeakerSet const &rhs) const;
        bool operator!=(IOSpeakerSet const &rhs) const;
    };
    
    struct ParameterInfo
    {
        ParamID     id_;                ///< unique identifier of this parameter (named tag too)
        String      title_;             ///< parameter title (e.g. "Volume")
        String      short_title_;       ///< parameter shortTitle (e.g. "Vol")
        String      units_;             ///< parameter unit (e.g. "dB")
        Int32       step_count_;                ///< number of discrete steps (0: continuous, 1: toggle, discrete value otherwise
        ///< (corresponding to max - min, for example: 127 for a min = 0 and a max = 127) - see \ref vst3parameterIntro)
        ParamValue  default_normalized_value_;  ///< default normalized value [0,1] (in case of discrete value: defaultNormalizedValue = defDiscreteValue / stepCount)
        UnitID      unit_id_;                   ///< id of unit this parameter belongs to (see \ref vst3UnitsIntro)
        
        bool can_automate_ = false;
        bool is_readonly_ = false;
        bool is_wrap_aound_ = false;
        bool is_list_ = false;
        bool is_program_change_ = false;
        bool is_bypass_ = false;
    };
    
    struct ProgramInfo
    {
        String name_;
        String plugin_name_;
        String plugin_category_;
        String instrument_;
        String style_;
        String character_;
        String state_type_;
        String file_path_string_type_;
        String file_name_;
    };
    
    struct ProgramList
    {
        //! the name of this list
        String name_;
        ProgramListID id_ = Steinberg::Vst::kNoProgramListId;
        std::vector<ProgramInfo> programs_;
    };
    
    struct UnitInfo
    {
        UnitID id_ = Steinberg::Vst::kRootUnitId;
        UnitID parent_id_ = Steinberg::Vst::kNoParentUnitId;
        String name_;
        ProgramList program_list_;
        Steinberg::Vst::ParamID program_change_param_ = Steinberg::Vst::kNoParamId;
    };
    
    using UnitInfoList = IdentifiedValueList<UnitInfo>;
    
    //! @note the value of channel_count_, speaker_, and is_active_ may be changed according to the bus state.
    struct BusInfo
    {
        //! Media type - has to be a value of @ref Steinberg::Vst::MediaTypes
        Steinberg::Vst::MediaTypes media_type_;
        
        //! input or output @ref Steinberg::Vst::BusDirections
        Steinberg::Vst::BusDirections direction_;
        
        //! number of channels
        //! For a bus of type MediaTypes::kEvent the channelCount corresponds
        //! to the number of supported MIDI channels by this bus
        Int32 channel_count_;
        
        //! name of the bus
        String name_;
        
        //! main or aux - has to be a value of @ref Steinerg::Vst::BusTypes
        Steinberg::Vst::BusTypes bus_type_;
        
        bool is_default_active_ = false;
        
        //! speaker arrangement of the bus.
        //! unused for event buses.
        SpeakerArrangement speaker_ = Steinberg::Vst::SpeakerArr::kEmpty;
        
        //! activation status
        bool is_active_ = false;
    };

public:
    //! コンストラクタ
    Vst3Plugin(Steinberg::IPluginFactory *factory, ClassInfo const &class_info);
    
    //! デストラクタ
	virtual ~Vst3Plugin();
    
    //! ファクトリ情報
    FactoryInfo const & GetFactoryInfo() const;
    //! コンポーネント情報
    ClassInfo const & GetComponentInfo() const;

    //! プラグイン名を返す
	String GetPluginName() const;
    
    //! オーディオ入力チャンネル数（全 AudioBus 合計）を返す
	size_t	GetNumAudioInputs() const;

    //! オーディオ出力チャンネル数（全 AudioBus 合計）を返す
    size_t  GetNumAudioOutputs() const;
    
    //! パラメータ数を返す
    UInt32  GetNumParams() const;
    
    //! 指定した index のパラメータを返す
    ParameterInfo const & GetParameterInfoByIndex(UInt32 index) const;
    //! 指定した ID を持つパラメータを返す。見つからない場合は std::nullopt を返す
    std::optional<Vst3Plugin::ParameterInfo> FindParameterInfoByID(ParamID id) const;

    //! ユニット数を返す
    UInt32  GetNumUnitInfo() const;
    //! 指定した index のユニット情報を返す
    UnitInfo const & GetUnitInfoByIndex(UInt32 index) const;
    //! 指定した ID を持つユニットを返す。見つからない場合は std::nullopt を返す
    UnitInfo const & GetUnitInfoByID(UnitID id) const;
    
    //! 指定したタイプのバスの数を返す。
    UInt32  GetNumBuses(MediaTypes media, BusDirections dir) const;

    //! 指定したタイプと index に対応する Bus の情報を返す。
    BusInfo const & GetBusInfoByIndex(MediaTypes media, BusDirections dir, UInt32 index) const;

    //! 指定した index のパラメータの値を返す
    ParamValue GetParameterValueByIndex(UInt32 index) const;
    //! 指定した ID のパラメータの値を返す
    ParamValue GetParameterValueByID(ParamID id) const;
    //! 指定した index のパラメータに値を設定する
    void SetParameterValueByIndex(UInt32 index, ParamValue value);
    //! 指定した index のパラメータに値を設定する
    void SetParameterValueByID(ParamID id, ParamValue value);
    
    //! パラメータの値を文字列表現に変換する
    String ValueToStringByIndex(UInt32 index, ParamValue value);
    //! 文字列表現をパラメータの値に変換する
    ParamValue StringToValueTByIndex(UInt32 index, String string);
    
    //! パラメータの値を文字列表現に変換する
    String ValueToStringByID(ParamID id, ParamValue value);
    //! 文字列表現をパラメータの値に変換する
    ParamValue StringToValueByID(ParamID id, String string);
    
    //! 指定した Bus がアクティブかどうかを返す
    bool IsBusActive(MediaTypes media, BusDirections dir, UInt32 index) const;
    //! 指定した Bus のアクティブ状態を設定する
    void SetBusActive(MediaTypes media, BusDirections dir, UInt32 index, bool state = true);
    //! 指定した設定に対応するバスのうち、アクティブなバスの数を返す
    UInt32 GetNumActiveBuses(MediaTypes media, BusDirections dir) const;
    //! 指定したバスの Speaker 構成を返す
    SpeakerArrangement GetSpeakerArrangementForBus(BusDirections dir, UInt32 index) const;

    //! 現在 Vst3Plugin クラスに設定している、全 AudioBus のスピーカー構成を返す
    IOSpeakerSet GetSpeakerArrangements() const;
    //! 実際に VST3 プラグイン に設定した、 全 AudioBus のスピーカー構成を返す
    IOSpeakerSet GetAppliedSpeakerArrangements() const;
    //! スピーカー構成を Vst3Plugin クラスに設定する。（実際の VST3 プラグインへは反映されない）
    void SetSpeakerArrangements(IOSpeakerSet const &speakers);
    //! スピーカー構成を Vst3Plugin クラスに設定する。（実際の VST3 プラグインへは反映されない）
    void SetSpeakerArrangement(BusDirections dir, UInt32 index, SpeakerArrangement arr);
    //! Vst3Plugin クラスに設定したスピーカー構成を、実際のVST3 プラグインへ反映する
    /*! @return 結果を表す std::pair クラス
     *  first: エラーコード。成功時は kResultTrue が設定される。
     *  second: もしVST3 プラグインが、自身に適用可能なスピーカー構成の情報を返してきた場合はそれが設定されている。
     *  とくになければ GetSpeakerArrangements() と同じものが設定される。ｍ
     */
    std::pair<Steinberg::tresult, IOSpeakerSet> ApplySpeakerArrangement();

    //! 合成処理を開始／継続
	void	Resume();
    //! 合成処理を停止
	void	Suspend();
    //! 合成処理が開始しているかどうかを返す。
	bool	IsResumed() const;
    //! 合成処理の単位を設定する
	void	SetBlockSize(int block_size);
    //! サンプリングレートを設定する
	void	SetSamplingRate(int sampling_rate);
    
    //!　エディター画面を持っているかどうかを返す
    bool    HasEditor() const;

#if defined(_MSC_VER)
    using WindowHandle = HWND;
#else
    using WindowHandle = void *;
#endif
    
    //! プラグイン側からのウィンドウ関連の通知を受け取るリスナークラス
    class IPlugFrameListener
    {
    protected:
        IPlugFrameListener() {}
    public:
        virtual ~IPlugFrameListener() {}
        
        //! プラグイン側からの画面サイズ変更要求をホストに通知するコールバック
        virtual void OnResizePlugView(Steinberg::ViewRect const &newSize) = 0;
    };
    
    //! エディター画面を表示する
    /*! @param wnd ウィンドウハンドル
     *  @param listener プラグイン側からのウィンドウ関連の通知を受け取るリスナー
     *  @return 処理に成功したかどうかを返す
     *  @note listener の寿命はこの Vst3Plugin では管理されない。
     *  listener の所有者が、 CloseEditor() 呼び出しに listener を破棄する必要がある。
     */
    bool	OpenEditor		(WindowHandle wnd, IPlugFrameListener *listener);
    
    //! エディター画面を閉じる
	void	CloseEditor		();

    //! エディター画面を開いているかどうかを返す
	bool	IsEditorOpened	() const;

    //! エディター画面をリサイズ可能かどうかを返す
    bool    CanChangeEditorSize() const;
    
    //! エディター画面のサイズを返す
	Steinberg::ViewRect
			GetEditorSize() const;
    
    //! エディター画面のサイズを変更する
    void    SetEditorSize(Steinberg::ViewRect const &rc);
    
    //! 有効なエディター画面のサイズを取得する
    /*! @param rc [in,out] 判定したいエディター画面のサイズ。
     *  このサイズがプラグインのエディター画面として適用できなかった場合は、
     *  適用可能なサイズに合うようにこの関数の中で rc の値が修正される。
     *  @return 処理が成功したかどうかを返す。
     *  引数に与えた rc の値が有効なサイズが有効だったどうかを返すものではないことに注意。
     */
    bool    GetEffectiveEditorSize(Steinberg::ViewRect &rc);

    //! 指定した Unit が現在選択している Program のインデックスを返す。
    UInt32  GetProgramIndex(UnitID unit_id = 0) const;
    
    //! 指定した Unit が現在選択している Program を指定したインデックスのものに変更する。
    void    SetProgramIndex(UInt32 index, UnitID unit_id = 0);

	//! パラメータの変更を次回の再生フレームでAudioProcessorに送信して適用するために、
	//! 変更する情報をキューに貯める
	void	EnqueueParameterChange(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value);

	void	RestartComponent(Steinberg::int32 flag);

    //! 1フレーム分の合成処理を行う
	void Process(ProcessInfo &pi);
    
    struct DumpData
    {
        std::vector<char> processor_data_;
        std::vector<char> edit_controller_data_;
    };
    
    //! プラグイン状態を保存する
    std::optional<DumpData> SaveData() const;
    
    //! プラグイン状態を復元する
    void LoadData(DumpData const &dump);
    
    using Vst3PluginListenerService = IListenerService<IVst3PluginListener>;
    Vst3PluginListenerService & GetVst3PluginListenerService();
    
    using Vst3PluginDestructionListenerService = IListenerService<IVst3PluginDestructionListener>;
    Vst3PluginDestructionListenerService & GetVst3PluginDestructionListenerService();
    
private:
	std::unique_ptr<Impl> pimpl_;
    std::unique_ptr<HostContext> host_context_;
    ListenerService<IVst3PluginDestructionListener> vpdls_;
};

NS_HWM_END
