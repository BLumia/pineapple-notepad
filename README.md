# Pineapple Notepad

Pineapple Notepad aims to be a simple, lightweight, and easy to use notepad application that can be a cross-platform alternative to Notepad++. It's based on [Scintilla](https://www.scintilla.org/), [Qt](https://www.qt.io/) and [KDE Framework](https://develop.kde.org/products/frameworks/), available on Windows, Linux[^7] and macOS[^8].

[^7]: Before reaching the Open Source sponsor goal, it will only available on Debian 13, Ubuntu 26.04 and deepin 25 amd64 version. Please consider become a sponsor to make me be able to reach the Open Source goal so it will be available on more distros and make porting possible.
[^8]: Available since `0.2.0`. Bundle will not be signed.

## Pricing

Pineapple Notepad itself is free of charge, even for commercial purposes. Please review `LICENSE` file for details.

Note that it's *currently* closed source. See the "Open Source Goal" section if you want to support Pineapple Notepad and make it open source. Please consider donate!

## Feature Comparation Table

| Feature | `Pineapple Notepad` | `Notepad++` | `Notepad Next` | `Notepad--` | `Kate` |
| ------- | ------------------- | ----------- | -------------- | ----------- | ------ |
| Open Source |         ❔[^1] |      ✅     |      ✅        |    ❌      | ✅     |
| Cross Platform |      ✅     | ❌ (Win-only) |    ✅        |    ✅      | ✅     |
| Text Encoding |       ✅     |      ✅     |      ❌        |    ✅      | ✅     |
| Show All Characters | ✅     |      ✅     |      ⚠️[^2]    |    ⚠️[^2]  | ⚠️[^4] |
| Session Restore |     ✅     |      ✅     |      ✅        |    ✅      | ✅     |
| Hi-DPI Support |      ✅     |      ⚠️[^3] |      ✅        |    ⚠️[^3]  | ✅     |
| Dark Theme Support |  ✅     |      ✅     |      ❌        |    ✅      | ✅     |
| Side-by-Side View |   ℹ️[^5] |      ✅     |      ✅        |    ❌      | ✅     |
| Customizable Toolbar | ✅    |      ❌     |      ❌        |    ❌      | ✅     |
| Macro Recording |     ✅     |      ✅     |      ✅        |    ✅      | ❌     |
| Change History |      ✅     |      ✅     |      ❌        |    ✅      | ✅     |
| Smart Highlight [^6] | ✅    |      ✅     |      ✅        |    ✅      | ✅     |
| `tail -f` Mode |      ✅     |      ✅     |      ❌        |    ✅      | ℹ️[^9] |
| Search Result Pane |  ✅     |      ✅     |      ✅        |    ✅      | ✅     |
| Folder Pane |         ✅     |      ✅     |      ✅        |    ✅      | ✅     |
| Document Map |        ✅     |      ✅     |      ❌        |    ❌      | ✅     |
| "Run..." Dialog |     ✅     |      ✅     |      ❌        |    ❌      | ❌     |
| Interval Backup |     ✅     |      ✅     |      ❌        |    ✅      | ✅     |
| Quick Switcher[^12] | ✅     |      ✅     |      ❌        |    ❌      | ✅     |
| User Defined Language | ✅   |      ✅     |      ❌        |    ⚠️[^11] | ✅     |
| Editor Engine |    Scintilla  |   Scintilla |   Scintilla     | QScintilla | KTextEdit |
| Regex Engine | QRegularExpression | Boost.Regex | QRegularExpression | Boost.Regex [^10] | QRegularExpression |

[^1]: Currently not open-sourced. Planned to be Open Source once Open Source Sponsor Goal is reached (please consider donate!). Tier 2 sponsors can also get the full source code for building and auditing‌ purpose before the goal is reached.
[^2]: Some non-printable characters are not able to be shown.
[^3]: Some components have known issues with Hi-DPI support.
[^4]: I created an add-on for this feature, but currently showing line ending is not possible to implement.
[^5]: Currently only available as an experimental version in sponsor tier, will be available in future releases once Open Source Sponsor Goal is reached.
[^6]: Highlight selected word in the editor.
[^9]: Enable "Read-only" mode and also enable "Auto-reload" mode to get similar experience.
[^10]: According to their pre-existing legacy code and their current dynamic library, they are very likely reusing the same implementation that Notepad++ is using (BoostRegExSearch).
[^11]: Only support keyword-level customization based on Lexilla's cpp lexer.
[^12]: Use <kbd>Ctrl+Tab</kbd> to switch between recently viewed opened documents.

## Open Source Goal

Pineapple Notepad is currently not an open source project, but the goal is to make Pineapple Notepad open source. I've set-up a $150 Ko-fi goal that Pineapple Notepad will be released as an Open-Source project if the goal is reached. Please read [this post](https://ko-fi.com/post/Pineapple-Notepad-A-Cross-platform-Notepad--Alte-G2G71V8JPX) for details. If you're interested, please use the following links to support this project.

### Sponsor Benefits

As a token of gratitude, sponsors will be able to receive a sponsor license and unlock some nice-to-have features before we reach the open-source sponsorship goals. Additionally, depending on the sponsor's donation, sponsors might be able to get credited within the application and access the source code before the open-source sponsorship goals are reached. Details are as follows:

<details>

All listed benefits below only apply to sponsors who donate **before** we reach the Open Source goal.

### Tier 0: 2 USD

- Get a sponsor license, which can be used to unlock some nice-to-have features in the free release version. Currently, the following features are available:
  - Theme color editor (to tweak a specific color in a theme)
  - Customizable context menu (for the editor area)
- Get credited within the blog of the next release

### Tier 1: 10 USD

- All tier 0 rights, and
- For at least the next 10 versions, get credited within the application's "About" dialog at "Special Thanks" tab
- Get pre-release / sponsor-edition of Pineapple Notepad on itch.io (the sponsor license is full featured without requiring a sponsor license)

### Tier 2: 35 USD

- All tier 1 rights, and
- Get source code access on itch.io before the open-source sponsor goal is reached (for auditing and building from source)

</details>

*If you have questions about sponsor benefits, please reach out to me by creating an issue or drop me a message on Ko-fi or any other form that can be reached.*

### Funding

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/blumia)

[![Afdian](https://static.afdiancdn.com/static/img/logo/logo.png)Afdian](https://afdian.com/a/BLumia)

[itch.io](https://blumia.itch.io/pineapple-notepad)

### Related Open-Sourced Projects

Some associated projects are already open-sourced:

- [kf6redist](https://github.com/BLumia/kf6redist): KDE Framework redistribution building scripts and patches.
- [libintl](https://github.com/BLumia/libintl): Patched libintl for easier build with CMake.
- [icoutils-rs](https://github.com/BLumia/icoutils-rs): drop-in, portable replacement for icoutils's `icotool`.

## License

Pineapple Notepad is released under Pineapple Notepad End User License Agreement.

This section will be updated once the Open Source goal is reached.
