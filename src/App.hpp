#pragma once

#include <memory>
#include <bitset>

#include "./misc/SingleInstance.hpp"
#include "./plugin/vst3/Vst3Plugin.hpp"
#include "./config/Config.hpp"

NS_HWM_BEGIN

class App
:   public wxApp
,   public SingleInstance<App>
{
public:
    using SingleInstance<App>::GetInstance;
    
    App();
    ~App();
    
    //! VST3プラグインのモジュールファイル（*.vst3）をロードする
    bool LoadVst3Module(String path);
    //! 現在ロードしているモジュールファイル（*.vst3）をアンロードする
    void UnloadVst3Module();
    
    //! 現在ロードしているモジュールのIPluginFactoryで、指定したcidのVST3プラグインをロードする。
    bool LoadVst3Plugin(ClassInfo::CID cid);
    //! 現在ロードしているプラグインをアンロードする
    void UnloadVst3Plugin();
    
    //! ロードしたモジュールから構築したVst3PluginFactoryを返す。
    //! まだモジュールをロードしていない場合はnullptrが返る。
    Vst3PluginFactory * GetPluginFactory();
    //! ロードしたプラグインを返す。
    //! まだプラグインをロードしていない場合はnullptrが返る。
    Vst3Plugin * GetPlugin();
    
    //! モジュールのロード／アンロード状態の変更通知を受け取るリスナークラス
    class ModuleLoadListener : public IListenerBase {
    protected:
        ModuleLoadListener() {}
    public:
        virtual void OnAfterModuleLoaded(String path, Vst3PluginFactory *factory) {}
        virtual void OnBeforeModuleUnloaded(Vst3PluginFactory *factory) {}
    };
    using ModuleLoadListenerService = IListenerService<ModuleLoadListener>;
    ModuleLoadListenerService & GetModuleLoadListenerService();
    
    //! プラグインのロード／アンロード状態の変更通知を受け取るリスナークラス
    class PluginLoadListener : public IListenerBase {
    protected:
        PluginLoadListener() {}
    public:
        virtual void OnAfterPluginLoaded(Vst3Plugin *plugin) {}
        virtual void OnBeforePluginUnloaded(Vst3Plugin *plugin) {}
    };
    using PluginLoadListenerService = IListenerService<PluginLoadListener>;
    PluginLoadListenerService & GetPluginLoadListenerService();
    
    //! 読み込んだプラグインにノートオンを送る
    void SendNoteOn(Int32 note_number, Int32 velocity = 100);
    //! 読み込んだプラグインにノートオフを送る
    void SendNoteOff(Int32 note_number, Int32 off_velocity = 0);
    //! 再生中のノートをすべて停止する
    void StopAllNotes();

    //! オーディオ入力が有効かどうか
    bool IsAudioInputEnabled() const;
    //! オーディオ入力を有効／無効にする
    void EnableAudioInput(bool enable = true);
    
    //! オーディオ出力レベルの最小値(dB値)を返す。
    double GetAudioOutputMinLevel() const;
    //! オーディオ出力レベルの最大値(dB値)を返す。
    double GetAudioOutputMaxLevel() const;
    //! 現在設定しているオーディオ出力レベル(dB値)を返す。
    double GetAudioOutputLevel() const;
    //! オーディオ出力レベルを変更する。
    void SetAudioOutputLevel(double db);
    
    //! 再生中のノートを返す。
    std::bitset<128> GetPlayingNotes();
    
    //! オーディオデバイスを選択し、オープンに成功したらコンフィグファイルを更新する
    void SelectAudioDevice();
    
    Config & GetConfig();
    Config const & GetConfig() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    bool OnInit() override;
    int OnExit() override;
};

NS_HWM_END
