#include "AppConfig.h"
#include "ExplorerCommandHandler.h"

#include <new>

HINSTANCE g_hInst = nullptr;

class ExplorerCommandClassFactory final : public IClassFactory
{
public:
    ExplorerCommandClassFactory() = default;

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (!ppv)
            return E_POINTER;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
            *ppv = static_cast<IClassFactory *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        const ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override
    {
        if (!ppv)
            return E_POINTER;
        *ppv = nullptr;
        if (pUnkOuter)
            return CLASS_E_NOAGGREGATION;

        ExplorerCommandHandler *handler = new (std::nothrow) ExplorerCommandHandler();
        if (!handler)
            return E_OUTOFMEMORY;
        HRESULT hr = handler->QueryInterface(riid, ppv);
        handler->Release();
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL) override { return S_OK; }

private:
    LONG m_cRef = 1;
};

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinst;
        DisableThreadLibraryCalls(hinst);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    if (!IsEqualCLSID(rclsid, CLSID_PnotepadShellExt))
        return CLASS_E_CLASSNOTAVAILABLE;

    ExplorerCommandClassFactory *factory = new (std::nothrow) ExplorerCommandClassFactory();
    if (!factory)
        return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return S_FALSE;
}

STDAPI DllRegisterServer()
{
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, modulePath, MAX_PATH) == 0)
        return E_FAIL;

    wchar_t subkey[160];
    wsprintfW(subkey, L"Software\\Classes\\CLSID\\{%s}\\InprocServer32", PNOTEPAD_SHELL_EXT_CLSID_STRING);

    HKEY hKey = nullptr;
    const LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, 0,
                                        KEY_WRITE, nullptr, &hKey, nullptr);
    if (result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(result);

    const wchar_t apartment[] = L"Apartment";
    RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(apartment), sizeof(apartment));
    RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(modulePath),
                   static_cast<DWORD>((wcslen(modulePath) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return S_OK;
}

STDAPI DllUnregisterServer()
{
    wchar_t subkey[160];
    wsprintfW(subkey, L"Software\\Classes\\CLSID\\{%s}", PNOTEPAD_SHELL_EXT_CLSID_STRING);
    RegDeleteTreeW(HKEY_CURRENT_USER, subkey);
    return S_OK;
}
