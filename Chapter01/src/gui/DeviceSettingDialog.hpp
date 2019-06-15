#pragma once

NS_HWM_BEGIN

//! デバイス設定ダイアログを開く。
/*! オーディオ出力デバイスが一つも見つからないときは、オーディオ出力デバイスを選択できないので、
 *  ShowModal()は即座にwxID_CANCELで処理を戻す。
 */
wxDialog * CreateDeviceSettingDialog(wxWindow *parent);

NS_HWM_END
