#pragma once

#include <windows.h>
#include <shobjidl_core.h>

inline constexpr CLSID CLSID_PnotepadShellExt = {
    0x51B3ABEB, 0xD342, 0x4DD3, {0x8A, 0xC0, 0xD4, 0xC1, 0xFE, 0x61, 0xBB, 0x9F}};

inline constexpr GUID GUID_PnotepadShellExtCanonicalName = {
    0xDA484EDC, 0xE742, 0x4F21, {0xA1, 0x44, 0x29, 0x95, 0xFA, 0x77, 0x93, 0x0E}};

#define PNOTEPAD_SHELL_EXT_CLSID_STRING "51B3ABEB-D342-4DD3-8AC0-D4C1FE61BB9F"

extern HINSTANCE g_hInst;
