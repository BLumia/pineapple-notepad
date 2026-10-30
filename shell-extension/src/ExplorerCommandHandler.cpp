#include "ExplorerCommandHandler.h"
#include "AppConfig.h"
#include "resource.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <strsafe.h>

#include <string>
#include <wchar.h>

namespace {

std::wstring ModuleDirectory()
{
    wchar_t buffer[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(g_hInst, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return {};
    wchar_t *slash = wcsrchr(buffer, L'\\');
    if (slash)
        *slash = L'\0';
    return buffer;
}

std::wstring FindApplicationPath()
{
    const std::wstring dir = ModuleDirectory();
    if (dir.empty())
        return {};

    const std::wstring exe = dir + L"\\pnotepad.exe";
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
        return {};
    return exe;
}

LPWSTR DuplicateWString(const wchar_t *value)
{
    if (!value)
        return nullptr;
    const size_t length = wcslen(value);
    LPWSTR result = static_cast<LPWSTR>(CoTaskMemAlloc((length + 1) * sizeof(wchar_t)));
    if (result)
        StringCchCopyW(result, length + 1, value);
    return result;
}

} // namespace

ExplorerCommandHandler::ExplorerCommandHandler() = default;
ExplorerCommandHandler::~ExplorerCommandHandler() = default;

IFACEMETHODIMP ExplorerCommandHandler::QueryInterface(REFIID riid, void **ppv)
{
    if (!ppv)
        return E_POINTER;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IExplorerCommand))
        *ppv = static_cast<IExplorerCommand *>(this);
    else
        return E_NOINTERFACE;
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) ExplorerCommandHandler::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) ExplorerCommandHandler::Release()
{
    const ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
        delete this;
    return cRef;
}

IFACEMETHODIMP ExplorerCommandHandler::GetTitle(IShellItemArray *, LPWSTR *ppszName)
{
    if (!ppszName)
        return E_POINTER;
    *ppszName = nullptr;
    wchar_t buf[256];
    const int len = LoadStringW(g_hInst, IDS_MENU_TITLE, buf, 256);
    if (len <= 0)
        return E_FAIL;
    *ppszName = DuplicateWString(buf);
    return *ppszName ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP ExplorerCommandHandler::GetIcon(IShellItemArray *, LPWSTR *ppszIcon)
{
    if (!ppszIcon)
        return E_POINTER;
    *ppszIcon = nullptr;
    const std::wstring exe = FindApplicationPath();
    if (exe.empty())
        return E_NOTIMPL;
    *ppszIcon = DuplicateWString((exe + L",0").c_str());
    return *ppszIcon ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP ExplorerCommandHandler::GetToolTip(IShellItemArray *, LPWSTR *ppszInfotip)
{
    if (ppszInfotip)
        *ppszInfotip = nullptr;
    return E_NOTIMPL;
}

IFACEMETHODIMP ExplorerCommandHandler::GetCanonicalName(GUID *pguidCommandName)
{
    if (!pguidCommandName)
        return E_POINTER;
    *pguidCommandName = GUID_PnotepadShellExtCanonicalName;
    return S_OK;
}

IFACEMETHODIMP ExplorerCommandHandler::GetState(IShellItemArray *, BOOL, EXPCMDSTATE *pCmdState)
{
    if (!pCmdState)
        return E_POINTER;
    *pCmdState = ECS_ENABLED;
    return S_OK;
}

IFACEMETHODIMP ExplorerCommandHandler::Invoke(IShellItemArray *psiItemArray, IBindCtx *)
{
    if (!psiItemArray)
        return E_INVALIDARG;

    const std::wstring exe = FindApplicationPath();
    if (exe.empty())
        return E_FAIL;

    DWORD count = 0;
    HRESULT hr = psiItemArray->GetCount(&count);
    if (FAILED(hr))
        return hr;

    for (DWORD i = 0; i < count; ++i)
    {
        IShellItem *item = nullptr;
        if (FAILED(psiItemArray->GetItemAt(i, &item)))
            continue;
        wchar_t *pszPath = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath)
        {
            const std::wstring arguments = L"\"" + std::wstring(pszPath) + L"\"";
            CoTaskMemFree(pszPath);
            ShellExecuteW(nullptr, L"open", exe.c_str(), arguments.c_str(), nullptr, SW_SHOWNORMAL);
        }
        item->Release();
    }
    return S_OK;
}

IFACEMETHODIMP ExplorerCommandHandler::GetFlags(EXPCMDFLAGS *pFlags)
{
    if (!pFlags)
        return E_POINTER;
    *pFlags = ECF_DEFAULT;
    return S_OK;
}

IFACEMETHODIMP ExplorerCommandHandler::EnumSubCommands(IEnumExplorerCommand **ppenumCommands)
{
    if (ppenumCommands)
        *ppenumCommands = nullptr;
    return E_NOTIMPL;
}
