#pragma once

#include <memory>

#include "./misc/SingleInstance.hpp"
#include "./plugin/vst3/Vst3Plugin.hpp"

NS_HWM_BEGIN

class App
:   public wxApp
,   public SingleInstance<App>
{
public:
    using SingleInstance<App>::GetInstance;
    
    App();
    ~App();
    
    bool LoadVst3Module(String path);
    void UnloadVst3Module();
    
    //! ロードしたモジュールのIPluginFactoryで、指定したcidのVST3 Pluginを構築する。
    bool LoadVst3Plugin(ClassInfo::CID cid);
    void UnloadVst3Plugin();
    
    //! 構築したプラグインを返す。
    //! まだ構築していない場合はnullptrが返る。
    Vst3PluginFactory * GetPluginFactory();
    Vst3Plugin * GetPlugin();
    
    class ModuleLoadListener : public IListenerBase {
    protected:
        ModuleLoadListener() {}
    public:
        virtual void OnAfterModuleLoaded(String path, Vst3PluginFactory *factory) {}
        virtual void OnBeforeModuleUnloaded(Vst3PluginFactory *factory) {}
    };
    using ModuleLoadListenerService = IListenerService<ModuleLoadListener>;
    
    ModuleLoadListenerService & GetModuleLoadListenerService();
    
    class PluginLoadListener : public IListenerBase {
    protected:
        PluginLoadListener() {}
    public:
        virtual void OnAfterPluginLoaded(Vst3Plugin *plugin) {}
        virtual void OnBeforePluginUnloaded(Vst3Plugin *plugin) {}
    };
    using PluginLoadListenerService = IListenerService<PluginLoadListener>;
    
    PluginLoadListenerService & GetPluginLoadListenerService();
    
    void SendNoteOn(Int32 note_number, Int32 velocity = 100);
    void SendNoteOff(Int32 note_number, Int32 off_velocity = 0);
    void StopAllNotes();
    
    //! 再生中のノートを返す。
    std::bitset<128> GetPlayingNotes();
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    bool OnInit() override;
    int OnExit() override;
};

NS_HWM_END
