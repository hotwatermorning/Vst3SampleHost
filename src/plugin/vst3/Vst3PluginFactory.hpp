#pragma once

#include <array>
#include <memory>
#include <functional>
#include <string>

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>

#include "../../misc/SingleInstance.hpp"
#include "./FactoryInfo.hpp"
#include "./ClassInfo.hpp"

NS_HWM_BEGIN

class Vst3Plugin;

class Vst3PluginFactory
{
public:
	Vst3PluginFactory(String module_path);
	~Vst3PluginFactory();

	FactoryInfo const &
			GetFactoryInfo() const;

	//! count number of components where PClassInfo::category is kVstAudioEffectClass.
	size_t GetComponentCount() const;

	ClassInfo const &
			GetComponentInfo(size_t index);

	std::unique_ptr<Vst3Plugin>
			CreateByIndex(size_t index);

	std::unique_ptr<Vst3Plugin>
            CreateByID(ClassInfo::CID const &component_id);
    
    UInt32   GetNumLoadedPlugins() const;

private:
	class Impl;
	std::unique_ptr<Impl> pimpl_;
};
    
class Vst3PluginFactoryList final
:   public SingleInstance<Vst3PluginFactoryList>
{
public:
    Vst3PluginFactoryList();
    ~Vst3PluginFactoryList();
    
    std::shared_ptr<Vst3PluginFactory> FindOrCreateFactory(String module_path);
    
    //! 指定したVst3PluginFactory（このVst3PluginFactoryListでロード済みのもの）のパスを返す
    String GetModulePath(Vst3PluginFactory *p) const;
    
    //! Unload factories which not having any plugins.
    void Shrink();
    
private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

NS_HWM_END
