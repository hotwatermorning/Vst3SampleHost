#pragma once

#include <memory>

NS_HWM_BEGIN

struct PluginKeyEventHandler;

//! VST3プラグインのエディターウィンドウで処理されなかったキーイベントを
//! wxEVT_KEY_*イベントで処理できるようにするためのワークアラウンド
/*! このshared_ptrが有効な間、VST3プラグインのエディターウィンドウで処理されなかったキーイベントが
 *  wndに指定したウィンドウにフォーワードされる。
 *  @param wnd [in] VST3プラグインのエディターを開くときに親ウィンドウとして渡したホスト側のウィンドウ。
 *  @return VST3プラグインのエディターウィンドウで処理されなかったキーイベントをwndにフォーワードする処理を行うRAIIクラス。
 */
std::shared_ptr<PluginKeyEventHandler> CreatePluginKeyEventHandler(wxWindow *wnd);

NS_HWM_END
