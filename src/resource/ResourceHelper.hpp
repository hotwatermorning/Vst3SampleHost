#pragma once

#include "../misc/SingleInstance.hpp"

NS_HWM_BEGIN

//! 指定したパス階層のリソースファイルの場所をフルパスで返す。
String GetResourcePath(String path);
//! 指定したパス階層のリソースファイルの場所をフルパスで返す。
String GetResourcePath(std::vector<String> path_hierarchy);

template<class T>
T GetResourceAs(String path)
{
    return T(GetResourcePath(path));
}

template<class T>
T GetResourceAs(std::vector<String> path_hierarchy)
{
    return T(GetResourcePath(path_hierarchy));
}

NS_HWM_END
