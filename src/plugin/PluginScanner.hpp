#pragma once

#include <vector>

#include "../misc/LockFactory.hpp"
#include "../misc/SingleInstance.hpp"
#include "../misc/ListenerService.hpp"
#include "./vst3/Vst3PluginFactory.hpp"

NS_HWM_BEGIN

struct PluginScanner
:   SingleInstance<PluginScanner>
{
    PluginScanner();
    ~PluginScanner();
    
    std::vector<String> const & GetDirectories() const;
    void AddDirectories(std::vector<String> const &dirs);
    void SetDirectories(std::vector<String> const &dirs);
    void ClearDirectories();
    
    std::vector<ClassInfo> GetPluginList() const;
    void ClearPluginList();

    struct Listener : public IListenerBase
    {
    protected:
        Listener() {}
    public:
        virtual
        void OnScanningStarted(PluginScanner *scanner) {}
        
        virtual
        void OnScanningProgressUpdated(PluginScanner *scanner) {}
        
        virtual
        void OnScanningFinished(PluginScanner *scanner) {}
    };
    
    using IListenerService = IListenerService<Listener>;

    IListenerService & GetListeners();
    
    void ScanAsync();
    void Wait();
    void Abort();
    
private:
    class Traverser;
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

bool HasPluginCategory(ClassInfo const &desc, std::string category_name);
bool IsEffectPlugin(ClassInfo const &desc);
bool IsInstrumentPlugin(ClassInfo const &desc);

std::optional<ClassInfo::CID> to_cid(std::string str);

NS_HWM_END
