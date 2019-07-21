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

struct Destroyer
{
    void operator()(wxWindow* p) {
        p->Destroy();
    }
};

std::unique_ptr<IAboutDialog, Destroyer> CreateAboutDialog();

NS_HWM_END
