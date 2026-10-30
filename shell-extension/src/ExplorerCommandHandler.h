#pragma once

#include <windows.h>
#include <shobjidl_core.h>

class ExplorerCommandHandler final : public IExplorerCommand
{
public:
    ExplorerCommandHandler();
    ~ExplorerCommandHandler();

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP GetTitle(IShellItemArray *psiItemArray, LPWSTR *ppszName) override;
    IFACEMETHODIMP GetIcon(IShellItemArray *psiItemArray, LPWSTR *ppszIcon) override;
    IFACEMETHODIMP GetToolTip(IShellItemArray *psiItemArray, LPWSTR *ppszInfotip) override;
    IFACEMETHODIMP GetCanonicalName(GUID *pguidCommandName) override;
    IFACEMETHODIMP GetState(IShellItemArray *psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE *pCmdState) override;
    IFACEMETHODIMP Invoke(IShellItemArray *psiItemArray, IBindCtx *pbc) override;
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS *pFlags) override;
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand **ppenumCommands) override;

private:
    LONG m_cRef = 1;
};
