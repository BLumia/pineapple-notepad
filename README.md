# Pineapple Notepad

Pineapple Notepad aims to be a simple, lightweight, and easy to use notepad application that can be a cross-platform alternative to Notepad++. It's based on [Scintilla](https://www.scintilla.org/), [Qt](https://www.qt.io/) and [KDE Framework](https://develop.kde.org/products/frameworks/), and plan to be available on Windows, Linux and macOS.

## Pricing

Pineapple Notepad itself will be free of charge. Note that it *might* be closed source at launch. See the following section if you want to support Pineapple Notepad and make it open source.

## Feature Comparation Table

| Feature | `Pineapple Notepad` | `Notepad++` | `Notepad Next` | `Notepad--` | `Kate` |
| ------- | ------------------- | ----------- | -------------- | ----------- | ------ |
| Open Source |         ❔[^1] |      ✅     |      ✅        |    ❌      | ✅     |
| Cross Platform |      ✅     | ❌ (Win-only) |    ✅        |    ✅      | ✅     |
| Text Encoding |       ✅     |      ✅     |      ❌        |    ✅      | ✅     |
| Show All Characters | ✅     |      ✅     |      ⚠️[^2]    |    ⚠️[^2]  | ❌[^5] |
| Session Restore |     ✅     |      ✅     |      ✅        |    ✅      | ✅     |
| Hi-DPI Support |      ✅     |      ⚠️[^3] |      ✅        |    ⚠️[^3]  | ✅     |
| Dark Theme Support |  ✅     |      ✅     |      ❌        |    ❌      | ✅     |
| Side-by-Side View |   ❌[^6] |      ✅     |      ⚠️[^4]    |    ❌      | ✅     |
| Customizable Toolbar | ✅    |      ❌     |      ❌        |    ❌      | ✅     |
| Macro Recording |     ✅     |      ✅     |      ✅        |    ✅      | ❌     |
| Editor Engine |    Scintilla  |   Scintilla |   Scintilla     | QScintilla | KTextEdit |

[^1]: Currently not released yet. Planned to be Open Source once Open Source Sponsor Goal is reached.
[^2]: Some non-printable characters are not able to be shown.
[^3]: Some components have known issues with Hi-DPI support.
[^4]: Available as custom-tailored docking system.
[^5]: I created an add-on for this feature, but currently showing line ending is not possible to implement.
[^6]: Not planned in the initial release, might be added in the future.

## Open Source Goal

Pineapple Notepad is currently not an open source project, but the goal is to make Pineapple Notepad open source. I've set-up a $150 Ko-fi goal that Pineapple Notepad will be released as an Open-Source project if the goal is reached. Please read [this post](https://ko-fi.com/post/Pineapple-Notepad-A-Cross-platform-Notepad--Alte-G2G71V8JPX) for details. If you're interested, please use the following links to support this project.

### Sponsor Benefits

Before the goal is reached, if you support Pineapple Notepad for equial or more than $10, you will be credited within the application's About dialog as a kickstart supporter. The name will be kept for at least 10 releases, after that, the name might be moved a standalone `THANKS` file in the source repository.

*If you have questions about sponsor benefits, please reach out to me by creating an issue or drop me a message on Ko-fi or any other form that can be reached.*

### Funding

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/blumia)

[![Afdian](https://static.afdiancdn.com/static/img/logo/logo.png)Afdian](https://afdian.com/a/BLumia)

## License

Depends on if we can reach the Open Source Sponsor Goal. This section will be updated once the goal is reached.

Before that happens, Pineapple Notepad is planned to be released under the [CC-BY-ND-4.0](https://spdx.org/licenses/CC-BY-ND-4.0.html) license.
