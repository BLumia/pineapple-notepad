# Pineapple Notepad — Windows 11 新式右键菜单扩展

本项目为应用（`pnotepad.exe`）提供一个 Windows 11 **新式右键菜单**条目"使用 Pineapple Notepad 编辑"。
Windows 11 新式菜单要求应用具有"包身份（package identity）"且菜单项由 `IExplorerCommand` 提供，
因此涉及两部分：

1. `shell-extension/src/` — 一个独立 COM DLL（`pnotepadshellext.dll`），实现 `IExplorerCommand`。
2. `dist/msix/` — 把 `install-msvc/bin` 中的应用负载 + 该 DLL 打包成 MSIX 的 PowerShell 脚本。

完整 MSIX 方案下应用无需为虚拟化改代码（前提是不往 exe 目录写文件；配置/日志请写 AppData）。

---

## 与主项目一同构建（MSVC）

shell 扩展 DLL 已整合进主项目 CMake 构建流程，**仅在 MSVC 环境下**自动随项目本体一同构建并安装。

```powershell
# 正常构建主项目（详见项目根目录 build.ps1）
.\build.ps1 -Offline
```

构建完成后 `pnotepadshellext.dll` 会随 `pnotepad.exe` 一起出现在 `install-msvc\bin\` 中。

> 在非 MSVC 环境（如 MSYS2/MinGW）下，此子目录会被跳过，不会影响构建。

### 单独构建 DLL（可选）

如需仅构建 DLL（例如用于调试），可手动调用：

```powershell
cmake -S shell-extension -B shell-extension/build -A x64
cmake --build shell-extension/build --config Release
```

产物：`shell-extension/build/Release/pnotepadshellext.dll`

本地调试（可选）：可以先 `regsvr32 pnotepadshellext.dll` 把它注册进**经典**右键菜单测试行为，
确认 `Invoke` 能正确启动 `pnotepad.exe` 后再打包。不需要时用 `regsvr32 /u` 注销。

---

## 打包脚本 `make-msix.ps1`

脚本会：读取 `install-msvc/bin`（默认负载目录）→ 拷贝全部文件 + `pnotepadshellext.dll` →
生成 `AppxManifest.xml`（自动探测主 exe、替换身份/扩展信息）→ `makeappx` 打包 →
`signtool` 签名 →（可选）`Add-AppxPackage` 安装。

MSIX 产物默认输出到 `install-msvc/msix/`。

### 基本用法

```powershell
# 打包不签名的 MSIX（默认）
powershell -ExecutionPolicy Bypass -File dist\msix\make-msix.ps1

# 打包自签名并安装（需管理员或信任证书）
powershell -ExecutionPolicy Bypass -File dist\msix\make-msix.ps1 -Sign -Install

# 重新安装时先移除旧包
powershell -ExecutionPolicy Bypass -File dist\msix\make-msix.ps1 -Sign -Install -ForceRemoveOld
```

### 参数说明

| 参数 | 默认值 | 说明 |
|---|---|---|
| `-InstallDir` | `<项目根>\install-msvc\bin` | 应用负载目录（含 `pnotepad.exe` 及其 DLL/插件） |
| `-Dll` | 优先取 `install-msvc\bin\pnotepadshellext.dll`，否则搜索 `build-msvc` | `pnotepadshellext.dll` 路径 |
| `-OutDir` | `<项目根>\install-msvc\msix` | 输出目录 |
| `-PfxPath` | 无 | 真实代码签名证书 `.pfx`；提供后不再生成自签名证书 |
| `-PfxPassword` | 无 | PFX 密码（无密码可省略） |
| `-SelfSignedSubject` | `PineappleNotepadTest` | 自签名证书 CN 名 |
| `-PackageName` | `net.blumia.PineappleNotepad` | 包身份名（需唯一，字母数字加点） |
| `-DisplayName` | `Pineapple Notepad` | 包显示名 |
| `-PublisherDisplayName` | `Gary Wang` | 发行者显示名 |
| `-Version` | `1.0.0.0` | 包版本 |
| `-AppId` | `PineappleNotepad` | Application Id |
| `-Verb` | `PineappleNotepadEdit` | 菜单命令 Id（**仅字母数字**，不能含点号） |
| `-ItemTypes` | `*` | 右键目标：`*` 所有文件 / `.txt` 扩展名 / `Directory` |
| `-Clsid` | `51B3ABEB-...` | 必须与 `src/AppConfig.h` 一致 |
| `-Arch` | `x64` | 查找 Windows SDK 工具用的架构 |
| `-IconPath` | `icons\pineapple-notepad.svg` | MSIX 图标来源；SVG 需系统已安装 magick / inkscape / rsvg-convert 之一，否则回退到 PNG |
| `-Sign` | 关 | 生成自签名证书并签名（默认不签名） |
| `-NoTimestamp` | 关 | 跳过时间戳服务器 |
| `-TimestampServer` | `http://timestamp.digicert.com` | 时间戳服务器 |
| `-Install` | 关 | 打包后调用 `Add-AppxPackage` |
| `-ForceRemoveOld` | 关 | 安装前卸载同名旧包 |

### 证书与签名

**默认：不签名。** 直接运行脚本即生成未签名 MSIX 包，适合本地快速测试。
按微软文档，含可执行内容的未签名包必须以**管理员身份**运行 PowerShell 安装
（配合 `-AllowUnsigned`），**不需要**开启开发人员模式。无法用于任何正式分发。

**自签名测试证书（`-Sign`）：** 生成 `install-msvc/msix/cert/PineappleNotepadTest.pfx`（密码 `PineappleNotepadTest123`），
并导出 `install-msvc/msix/cert/PineappleNotepadTest.cer`。本机安装签名包前需先信任证书或开启开发人员模式。

```powershell
powershell -ExecutionPolicy Bypass -File dist\msix\make-msix.ps1 -Sign -Install
```

**真实证书（正式分发）：**

```powershell
powershell -ExecutionPolicy Bypass -File dist\msix\make-msix.ps1 `
  -PfxPath C:\certs\real.pfx -PfxPassword "你的密码" -Install -ForceRemoveOld
```

脚本会读取证书的 Subject 自动填入清单的 `Publisher`（必须与证书完全一致），无需手改。

### 安装前提（本机测试，按场景选一）

- 未签名（默认）：以管理员身份运行 PowerShell，脚本使用 `-AllowUnsigned` 安装
- 自签名（`-Sign`）：信任证书——双击 `install-msvc/msix/cert/PineappleNotepadTest.cer` → 安装证书 → 当前用户 → 受信任的根证书颁发机构
- 真实证书（`-PfxPath`）：使用受信任的代码签名证书

安装后若菜单未出现，注销/重启资源管理器（任务管理器 → Windows 资源管理器 → 重新启动）。

### 卸载

```powershell
Get-AppxPackage -Name net.blumia.PineappleNotepad | Remove-AppxPackage
```

---

## 自定义

| 想改什么 | 改哪里 |
|---|---|
| 菜单文字（多语言本地化） | `src/resource.rc` 的 `STRINGTABLE`（每个 `LANGUAGE` 块一份；英文兜底用 `LANG_ENGLISH`，不要用 `LANG_NEUTRAL`），然后重新 `cmake --build` |
| 显示名 / 包名 / 发行者 | 打包参数 `-DisplayName` `-PackageName` `-PublisherDisplayName` |
| 右键出现的对象 | `-ItemTypes`（`*` 所有文件；`.txt` 仅文本；可加 `Directory`） |
| 命令 Id | `-Verb`（仅 `[A-Za-z0-9]`） |
| CLSID | `src/AppConfig.h` 与 `-Clsid` 同步修改 |

包版本每次升级必须增大 `-Version`，否则无法覆盖安装。

### 菜单文字本地化

菜单文案来自 `src/resource.rc` 的 `STRINGTABLE`（资源 ID `IDS_MENU_TITLE`，定义于
`src/resource.h`）。`GetTitle` 通过 `LoadStringW` 读取，语言跟随系统 UI：

- `rc` 文件顶部 `#pragma code_page(65001)` 让资源编译器按 UTF-8 解释源文件（源码须存为 UTF-8）；
- 每个 `LANGUAGE` 块对应一种语言；英文兜底**必须**用 `LANG_ENGLISH, SUBLANG_ENGLISH_US`——
  **不要用 `LANG_NEUTRAL`**，否则即便线程首选 UI 语言是中文，资源加载器也会回退到 neutral 英文块；
- `LoadStringW` 按线程/进程首选 UI 语言匹配对应语言块，匹配不到时回退——
  无需写任何判断语言的代码。新增语言只需追加一个 `LANGUAGE` 块。

---

## 常见问题

| 现象 | 原因/解决 |
|---|---|
| 安装报 `0x800B0109` | 证书不受信任 → 信任 `install-msvc/msix/cert/PineappleNotepadTest.cer` 或换真实证书 |
| 安装报 `0x80073CF9` | 同上（发布者无法验证） |
| 安装报 `0x80073D05`/版本错误 | 同名旧包存在 → 先 `Remove-AppxPackage` 或加 `-ForceRemoveOld` |
| 菜单没出现 | 检查是否装的是新包；重启资源管理器或注销 |
| 未签名装不上 | 脚本已写入未签名命名空间 OID；用管理员 PowerShell 跑 `-Install`（无需 `-Sign`） |
| 想改右键目标后重新打包 | 传 `-ItemTypes` 重新跑脚本即可，无需重编 DLL |

---

## 分发注意

- 正式对外分发请使用受信任的代码签名证书（`-PfxPath`），或上架 Microsoft Store。
- 自签名证书 + 手动信任仅适合本机/内网测试。
- MSIX 安装后应用目录只读：若应用原本会往 exe 目录写配置/日志，需改为写
  `QStandardPaths::AppDataLocation`（`%LocalAppData%`）等可写位置。
