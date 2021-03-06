#include "Vst3PluginFactory.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
#include <map>

#include <pluginterfaces/base/ftypes.h>
#include <public.sdk/source/vst/hosting/module.h>
#include "VstMAUtils.hpp"
#include "Vst3Plugin.hpp"
#include "../../misc/StrCnv.hpp"
#include "../../misc/LockFactory.hpp"

using namespace Steinberg;

NS_HWM_BEGIN

class Vst3PluginFactory::Impl
:   public IVst3PluginDestructionListener
{
public:
    Impl(String module_path);
    ~Impl();

    size_t    GetComponentCount() { return class_info_list_.size(); }
    ClassInfo const &
            GetComponent(size_t index) { return class_info_list_[index]; }

    FactoryInfo const &
            GetFactoryInfo() { return factory_info_; }

    IPluginFactory *
            GetFactory()
    {
        return factory_.get();
    }
    
    void OnAfterConstruction(Vst3Plugin *plugin) {
        loaded_plugins_.push_back(plugin);
        plugin->GetVst3PluginDestructionListenerService().AddListener(this);
    }
    
    void OnBeforeDestruction(Vst3Plugin *plugin) override {
        auto found = std::find(loaded_plugins_.begin(), loaded_plugins_.end(), plugin);
        assert(found != loaded_plugins_.end());
        loaded_plugins_.erase(found);
        plugin->GetVst3PluginDestructionListenerService().RemoveListener(this);
    }
    
    UInt32 GetNumLoadedPlugins() const {
        return loaded_plugins_.size();
    }

private:
    using Module = VST3::Hosting::Module::Ptr;
    Module module_;

    typedef void (PLUGIN_API *SetupProc)();

    using factory_ptr = vstma_unique_ptr<IPluginFactory>;
    factory_ptr                factory_;
    FactoryInfo                factory_info_;
    std::vector<ClassInfo>    class_info_list_;
    std::vector<Vst3Plugin const *> loaded_plugins_;
};

String FormatCid(ClassInfo::CID const &cid)
{
    int const reg_str_len = 48; // including null-terminator
    std::string reg_str(reg_str_len, '\0');
    FUID fuid = FUID::fromTUID(cid.data());
    fuid.toRegistryString(&reg_str[0]);
    return hwm::to_wstr(reg_str.c_str());
}

std::wstring
        ClassInfoToString(ClassInfo const &info)
{
    std::wstringstream ss;
    ss    << info.GetName()
        << L", " << FormatCid(info.GetCID())
        << L", " << info.GetCategory()
        << L", " << info.GetCardinality();

    if(info.HasClassInfo2()) {
        ss    << L", " << info.GetClassInfo2().GetSubCategories()
            << L", " << info.GetClassInfo2().GetVendor()
            << L", " << info.GetClassInfo2().GetVersion()
            << L", " << info.GetClassInfo2().GetSDKVersion()
            ;
    }
    return ss.str();
}

template<class Container>
void OutputClassInfoList(Container const &cont)
{
    HWM_DEBUG_LOG(L"--- Output Class Info ---");
    int count = 0;
    for(ClassInfo const &info: cont) {
        HWM_DEBUG_LOG(L"[" << count++ << L"] " << ClassInfoToString(info));
    }
}

std::wstring
        FactoryInfoToString(FactoryInfo const &info)
{
    std::wstringstream ss;
    ss    << info.GetVendor()
        << L", " << info.GetURL()
        << L", " << info.GetEmail()
        << std::boolalpha
        << L", " << L"Discardable: " << info.IsDiscardable()
        << L", " << L"License Check: "<< info.IsLicenseCheck()
        << L", " << L"Component Non Discardable: " << info.IsComponentNonDiscardable()
        << L", " << L"Unicode: " << info.IsUnicode();
    return ss.str();
}

void OutputFactoryInfo(FactoryInfo const &info)
{
    HWM_DEBUG_LOG(L"--- Output Factory Info ---");
    HWM_DEBUG_LOG(FactoryInfoToString(info));
}

Vst3PluginFactory::Impl::Impl(String module_path)
{
    std::string path = to_utf8(module_path);
    std::string error;

    Module mod = VST3::Hosting::Module::create(path, error);
    if(!mod) {
        throw std::runtime_error("cannot load library: " + error);
    }

    auto maybe_factory = queryInterface<IPluginFactory>(mod->getFactory().get());
    if(!maybe_factory) {
        throw std::runtime_error("Failed to get factory function");
    }

    auto factory = std::move(maybe_factory.right());
    
    PFactoryInfo loaded_factory_info;
    factory->getFactoryInfo(&loaded_factory_info);

    std::vector<ClassInfo> class_info_list;

    for(int i = 0; i < factory->countClasses(); ++i) 
    {
        if(auto f = queryInterface<IPluginFactory3>(factory)) 
        {
            PClassInfoW info;
            f.right()->getClassInfoUnicode(i, &info);
            class_info_list.emplace_back(info);
        } 
        else if(auto f = queryInterface<IPluginFactory2>(factory)) 
        {
            PClassInfo2 info;
            f.right()->getClassInfo2(i, &info);
            class_info_list.emplace_back(info);
        } 
        else 
        {
            PClassInfo info;
            factory->getClassInfo(i, &info);
            class_info_list.emplace_back(info);
        }
    }

    module_ = std::move(mod);
    factory_ = std::move(factory);
    factory_info_ = FactoryInfo(loaded_factory_info);
    class_info_list_ = std::move(class_info_list);

    OutputFactoryInfo(factory_info_);
    OutputClassInfoList(class_info_list_);
}

Vst3PluginFactory::Impl::~Impl()
{
    assert(loaded_plugins_.empty());
    
    factory_.reset();
    module_.reset();
}

Vst3PluginFactory::Vst3PluginFactory(String module_path)
{
    pimpl_ = std::unique_ptr<Impl>(new Impl(module_path));
}

Vst3PluginFactory::~Vst3PluginFactory()
{}

FactoryInfo const &
        Vst3PluginFactory::GetFactoryInfo() const
{
    return pimpl_->GetFactoryInfo();
}

//! PClassInfo::category が kVstAudioEffectClass のもののみ
size_t    Vst3PluginFactory::GetComponentCount() const
{
    return pimpl_->GetComponentCount();
}

ClassInfo const &
        Vst3PluginFactory::GetComponentInfo(size_t index)
{
    return pimpl_->GetComponent(index);
}

std::unique_ptr<Vst3Plugin>
        Vst3PluginFactory::CreateByIndex(size_t index)
{
    auto p = std::make_unique<Vst3Plugin>(pimpl_->GetFactory(), GetComponentInfo(index));
    pimpl_->OnAfterConstruction(p.get());
    return p;
}

std::unique_ptr<Vst3Plugin>
    Vst3PluginFactory::CreateByID(ClassInfo::CID const &component_id)
{
    for(size_t i = 0; i < GetComponentCount(); ++i) {
        if(component_id == GetComponentInfo(i).GetCID()) {
            return CreateByIndex(i);
        }
    }

    throw std::runtime_error("No specified id in this factory.");
}

UInt32 Vst3PluginFactory::GetNumLoadedPlugins() const {
    return pimpl_->GetNumLoadedPlugins();
}

class Vst3PluginFactoryList::Impl
{
public:
    LockFactory lf_;
    std::map<String, std::shared_ptr<Vst3PluginFactory>> table_;
};

Vst3PluginFactoryList::Vst3PluginFactoryList()
:   pimpl_(std::make_unique<Impl>())
{}

Vst3PluginFactoryList::~Vst3PluginFactoryList()
{}

std::shared_ptr<Vst3PluginFactory> Vst3PluginFactoryList::FindOrCreateFactory(String module_path)
{
    auto lock = pimpl_->lf_.make_lock();
    
    auto found = pimpl_->table_.find(module_path);
    if(found == pimpl_->table_.end()) {
        std::shared_ptr<Vst3PluginFactory> factory;
        try {
            factory = std::make_shared<Vst3PluginFactory>(module_path);
        } catch(std::exception &e) {
            HWM_ERROR_LOG(L"Failed to create a Vst3PluginFactory [" << module_path << L"]: " << to_wstr(e.what()));
            return {};
        }
        
        found = pimpl_->table_.emplace(module_path, factory).first;
    }
    
    return found->second;
}

String Vst3PluginFactoryList::GetModulePath(Vst3PluginFactory *p) const
{
    for(auto const &entry: pimpl_->table_) {
        if(entry.second.get() == p) { return entry.first; }
    }
    
    return {};
}

void Vst3PluginFactoryList::Shrink()
{
    std::map<String, std::shared_ptr<Vst3PluginFactory>> tmp;
    
    for(auto it = pimpl_->table_.begin(), end = pimpl_->table_.end();
        it != end;
        )
    {
        if(it->second->GetNumLoadedPlugins() == 0) {
            it = pimpl_->table_.erase(it);
        } else {
            ++it;
        }
    }
}

NS_HWM_END
