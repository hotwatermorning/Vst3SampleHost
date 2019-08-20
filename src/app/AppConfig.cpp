#include "./AppConfig.hpp"

NS_HWM_BEGIN

wchar_t const * const kAppName =
#include "./AppName.inc"
;

wchar_t const * const kAppVersion =
#include "./Version.inc"
;

wchar_t const * const kAppCommitID =
#include "./CommitID.inc"
;

NS_HWM_END
