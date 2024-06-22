# Pineapple Notepad？

## 说明

这是一个用来实验 KDE Framework 的项目（主要是 Widgets 相关的组件），此项目本身（至少目前）不是一个能用的文本编辑器，甚至可以说实际上什么功能都没有。

现阶段，MSYS2 仓库的 KF6 组件已足以构建基本的应用程序，并包括一些优点。

- KXmlGui: 提供了一个便于创建与管理 QAction 的框架，可以便于将 Action 注册到菜单栏、工具栏，甚至还有一个命令搜索条。工具栏中的项目可由用户自由调整。`xxxxui.rc`
- KConfig: 提供了一个基于 XML 文件生成配置管理工具类的解决方案。`xxx.kcfg`（配置文件结构本体）与 `xxx.kcfgc`（如何生成工具类的配置）
- KConfigWidgets: 一系列配套组件，包括：
  - KConfigDialog: 提供一个包括多个页的配置对话框，将若干页面加入此对话框，且根据 KConfig 生成的工具类自动更新每个页面的控件（页面内的控件需使用 `kcfg_配置名称` 作为对象名称）。
  - KHamburgerMenu: 汉堡菜单
  - KStyleManager: 用于初始化 breeze 主题（和配置似乎没啥关系..）
  - KColorSchemeMenu: 提供切换主题的菜单项（MSYS2 下，KColorSchemeMenu 实际列出的可用主题只有默认和暗色两个？）
- KIconTheme: 初始化图标主题，需要在 QApplication 构造之前调用初始化，并且看上去需要配合什么别的东西才能做到颜色正确...
