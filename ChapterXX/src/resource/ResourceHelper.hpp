#pragma once

#include "../misc/SingleInstance.hpp"

NS_HWM_BEGIN

//! 指定したパス階層のリソースファイルの場所をフルパスで返す。
String GetResourcePath(String path);
//! 指定したパス階層のリソースファイルの場所をフルパスで返す。
String GetResourcePath(std::vector<String> path_hierarchy);
//! コンフィグファイルの場所をフルパスで返す。
/*! このファイルは、以下のようなユーザーの設定ファイル用ディレクトリに置かれる。
 *    * Win: "C:\Users\<UserName>\AppData\Roaming\Vst3SampleHost\Vst3SampleHost.conf"
 *    * Mac: "~/Library/Preferences/Vst3SampleHost/Vst3SampleHost.conf"
 */
String GetConfigFilePath();

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
