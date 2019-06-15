#pragma once

#include <memory>
#include <bitset>

#include "./misc/ListenerService.hpp"
#include "./misc/SingleInstance.hpp"
#include "./file/Config.hpp"

NS_HWM_BEGIN

class App
:   public wxApp
,   public SingleInstance<App>
{
public:
    using SingleInstance<App>::GetInstance;
    
    App();
    ~App();
    
    class PlaybackOptionChangeListener : public IListenerBase {
    protected:
        PlaybackOptionChangeListener() {}
    public:
        //! オーディオデバイスの状態が変更になって、オーディオ入力が可能かどうかが変化したときに呼ばれるコールバック
        virtual void OnAudioInputAvailabilityChanged(bool available) {}
        //! オーディオ入力が可能な状態で、その有効／無効を切り替えたときに呼ばれるコールバック
        virtual void OnAudioInputEnableStateChanged(bool enabled) {}
    };
    using PlaybackOptionChangeListenerService = IListenerService<PlaybackOptionChangeListener>;
    PlaybackOptionChangeListenerService & GetPlaybackOptionChangeListenerService();
    
    //! 読み込んだプラグインにノートオンを送る
    void SendNoteOn(Int32 note_number, Int32 velocity = 100);
    //! 読み込んだプラグインにノートオフを送る
    void SendNoteOff(Int32 note_number, Int32 off_velocity = 0);
    //! 再生中のノートをすべて停止する
    void StopAllNotes();

    //! オーディオ入力を有効にできるかどうか。
    /*! オーディオ入力デバイスをオープンしているときはtrueを返す。
     */
    bool CanEnableAudioInput() const;
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
    
    void LoadProjectFile(String path_to_load);
    void SaveProjectFile(String path_to_save);
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    bool OnInit() override;
    int OnExit() override;
};

NS_HWM_END
