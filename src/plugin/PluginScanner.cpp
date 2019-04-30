#include "PluginScanner.hpp"

#include <atomic>
#include <thread>

#include <wx/dir.h>

#include "../misc/ScopeExit.hpp"
#include "../misc/StrCnv.hpp"
#include "../misc/ListenerService.hpp"
#include <pluginterfaces/vst/ivstaudioprocessor.h>

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
:   public wxDirTraverser
{
public:
    Traverser(PluginScanner &owner)
    :   owner_(owner)
    {}
    
    bool LoadFactory(String const &path)
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
    
    wxDirTraverseResult OnFile(wxString const &filename) override
    {
#if SMTG_OS_MACOS == 0
        if(filename.EndsWith(L"vst3")) { LoadFactory(filename.ToStdWstring()); }
#endif
        return wxDIR_CONTINUE;
    }
    
    wxDirTraverseResult OnDir(wxString const &dirname) override
    {
#if SMTG_OS_MACOS
        if(dirname.EndsWith(L"vst3")) { LoadFactory(dirname.ToStdWstring()); }
#endif
        return wxDIR_CONTINUE;
    }
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
            if(wxDir::Exists(path) == false) { continue; }
            
            wxDir dir(path);
            
            try {
                dir.Traverse(tr);
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
