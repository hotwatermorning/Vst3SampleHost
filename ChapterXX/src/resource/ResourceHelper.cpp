#include "ResourceHelper.hpp"

#include <wx/stdpaths.h>
#include <wx/filename.h>

NS_HWM_BEGIN

constexpr wchar_t const * kConfigDirName = L"Vst3SampleHost";
constexpr wchar_t const * kConfigFileName = L"Vst3SampleHost.conf";

//! Get resource file path specified by the path hierarchy.
String GetResourcePath(String path)
{
    assert(path.size() > 0);
    
    if(path.front() != L'/' && path.front() != L'\\') {
        path = L'/' + path;
    }
    
#if defined(_MSC_VER)
    auto exe_path = wxFileName::DirName(wxStandardPaths::Get().GetExecutablePath());
    exe_path.RemoveLastDir();
    return exe_path.GetFullPath().ToStdWstring() + L"Resource" + path;
#else
    return wxStandardPaths::Get().GetResourcesDir().ToStdWstring() + path;
#endif
}

//! Get resource file path specified by the path hierarchy.
String GetResourcePath(std::vector<String> path_hierarchy)
{
    assert(path_hierarchy.empty() == false &&
           std::all_of(path_hierarchy.begin(),
                       path_hierarchy.end(),
                       [](auto const &x) { return x.size() > 0; }
                       ));
    
    String concat;
    
    for(auto x: path_hierarchy) {
        if(x.front() != L'/' && x.front() != L'\\') {
            x = L'/' + x;
        }
        if(x.back() == L'/' || x.back() == L'\\') {
            x.pop_back();
        }
        
        concat += x;
    }
    
    return GetResourcePath(concat);
}

String GetConfigFilePath()
{
    auto dir = wxFileName::DirName(wxStandardPaths::Get().GetUserConfigDir());
    dir.AppendDir(kConfigDirName);
    dir.SetFullName(kConfigFileName);
    
    return dir.GetFullPath().ToStdWstring();
}

NS_HWM_END
