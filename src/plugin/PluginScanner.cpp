#include "PluginScanner.hpp"

#include <atomic>
#include <thread>

#include "../misc/ScopeExit.hpp"
#include "../misc/StrCnv.hpp"
#include "../misc/ListenerService.hpp"
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <FL/filename.H>
#include <FL/fl_utf8.h>

NS_HWM_BEGIN

std::optional<ClassInfo::CID> to_cid(String str)
{
    if(str.length() != ClassInfo::CID{}.size()) { return std::nullopt; }
 
    auto tmp = to_utf8(str);
    ClassInfo::CID cid = {};
    std::copy_n(tmp.begin(), tmp.size(), cid.begin());
    return cid;
}

template<class Container>
bool Contains(Container &c, ClassInfo::CID const &cid) {
    return std::any_of(c.begin(), c.end(), [&cid](ClassInfo const &info) {
        return info.cid() == cid;
    });
}

struct PluginScanner::Impl
{
    Impl()
    {
        scanning_ = false;
    }
    
    std::vector<String> path_to_scan_;
    LockFactory lf_;
    std::vector<ClassInfo> pds_;
    std::thread th_;
    std::atomic<bool> scanning_;
    ListenerService<PluginScanner::Listener> listeners_;
};

class PluginScanner::Traverser
{
public:
    Traverser(PluginScanner &owner)
    :   owner_(owner)
    {}
    
    void ScanDir(std::string path)
    {
        struct dirent **list;
        int const num = fl_filename_list(path.c_str(), &list, fl_alphasort);
        if(num < 0) {
            hwm::dout << "error while scanning file list." << std::endl;
            return;
        }
        
        HWM_SCOPE_EXIT([&list, num] {
            fl_filename_free_list(&list, num);
        });
        
        auto const is_ext_vst3 = [](auto name) {
            return fl_filename_ext(name) == std::string(".vst3");
        };
        
        for(int i = 0; i < num; ++i) {
            dirent * &entry = list[i];
            if(fl_filename_isdir(entry->d_name)) {
#if SMTG_OS_MACOS != 0
                if(is_ext_vst3(entry->d_name)) {
                    bool const is_plugin = ScanPlugin(entry->d_name);
                    if(is_plugin) { continue; }
                }
#endif
                ScanDir(entry->d_name);
            } else {
#if SMTG_OS_MACOS == 0
                struct stat st;
                fl_stat(entry->d_name, &st);
                
                if(S_ISREG(st.st_mode) && is_ext_vst3(entry->d_name)) {
                    ScanPlugin(entry->d_name);
                }
#endif
            }
        }
    }
    
    bool ScanPlugin(std::string const &path)
    {
        auto factory_list = Vst3PluginFactoryList::GetInstance();
        auto factory = factory_list->FindOrCreateFactory(to_wstr(path));

        if (!factory) {
            return false;
        }

        auto const num = factory->GetComponentCount();
        for (int i = 0; i < num; ++i) {
            auto info = factory->GetComponentInfo(i);

            //! カテゴリがkVstAudioEffectClassでないComponentは、オーディオプラグインではないので無視する。
            if (info.category() != hwm::to_wstr(kVstAudioEffectClass)) {
                continue;
            }

            auto lock = owner_.pimpl_->lf_.make_lock();
            auto &pds = owner_.pimpl_->pds_;

            if (Contains(pds, info.cid())) { continue; }
            
            pds.push_back(info);

            lock.unlock();
            
            owner_.pimpl_->listeners_.Invoke([this](auto *li) {
                li->OnScanningProgressUpdated(&owner_);
            });
        }
        
        return true;
    }
    
private:
    PluginScanner &owner_;
};

PluginScanner::PluginScanner()
:   pimpl_(std::make_unique<Impl>())
{}

PluginScanner::~PluginScanner()
{
    Wait();
}

std::vector<String> const & PluginScanner::GetDirectories() const
{
    auto lock = pimpl_->lf_.make_lock();
    return pimpl_->path_to_scan_;
}

void PluginScanner::AddDirectories(std::vector<String> const &dirs)
{
    auto lock = pimpl_->lf_.make_lock();
    pimpl_->path_to_scan_.insert(pimpl_->path_to_scan_.end(),
                                 dirs.begin(), dirs.end()
                                 );
}

void PluginScanner::SetDirectories(std::vector<String> const &dirs)
{
    auto lock = pimpl_->lf_.make_lock();
    pimpl_->path_to_scan_ = dirs;
}

void PluginScanner::ClearDirectories()
{
    auto lock = pimpl_->lf_.make_lock();
    pimpl_->path_to_scan_.clear();
}

std::vector<ClassInfo> PluginScanner::GetPluginList() const
{
    auto lock = pimpl_->lf_.make_lock();
    return pimpl_->pds_;
}

void PluginScanner::ClearPluginList()
{
    auto lock = pimpl_->lf_.make_lock();
    pimpl_->pds_.clear();
}

PluginScanner::IListenerService & PluginScanner::GetListeners()
{
    return pimpl_->listeners_;
}

void PluginScanner::ScanAsync()
{
    bool expected = false;
    if(pimpl_->scanning_.compare_exchange_strong(expected, true) == false) {
        return;
    }

    Wait();
    
    pimpl_->th_ = std::thread([this] {
        pimpl_->listeners_.Invoke([this](auto *li) {
            li->OnScanningStarted(this);
        });
        
        auto path_to_scan = GetDirectories();
        
        Traverser tr(*this);
        for(auto path: path_to_scan) {
            auto const utf8path = to_utf8(path);
            if(fl_filename_isdir(utf8path.c_str()) == false) { continue; }

            try {
                tr.ScanDir(utf8path);
            } catch(std::exception &e) {
                hwm::dout << "Failed to traverse plugin directories: " << e.what() << std::endl;
            }
        }
        
        pimpl_->scanning_ = false;

        pimpl_->listeners_.Invoke([this](auto *li) {
            li->OnScanningFinished(this);
        });
    });
}

void PluginScanner::Wait()
{
    if(pimpl_->th_.joinable()) {
        pimpl_->th_.join();
    }
}

bool HasPluginCategory(ClassInfo const &info, std::string category_name)
{
    auto tmp = to_wstr(category_name);
    if(info.has_classinfo2()) {
        return info.classinfo2().sub_categories().find(tmp) != std::string::npos;
    } else {
        return false;
    }
}

bool IsEffectPlugin(ClassInfo const &info)
{
    return HasPluginCategory(info, "Fx");
}

bool IsInstrumentPlugin(ClassInfo const &info)
{
    return HasPluginCategory(info, "Inst");
}

NS_HWM_END
