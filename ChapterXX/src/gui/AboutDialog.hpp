#pragma once

NS_HWM_BEGIN

class IAboutDialog
:   public wxDialog
{
protected:
    template<class... Args>
    IAboutDialog(Args&&...);

public:
    virtual
    bool IsOk() const = 0;
};

IAboutDialog * CreateAboutDialog();

NS_HWM_END
