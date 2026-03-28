# Pineapple Notepad

Pineapple Notepad aims to be a simple, lightweight, and easy to use notepad application that can be a cross-platform alternative to Notepad++. It's based on [Scintilla](https://www.scintilla.org/), [Qt](https://www.qt.io/) and [KDE Framework](https://develop.kde.org/products/frameworks/), available on Windows, Linux[^7] and macOS[^8].

[^7]: Before reaching the Open Source sponsor goal, it will only available on deepin 25 amd64 version. Please consider become a sponsor to make me be able to reach the Open Source goal so it will be available on more distros and make porting possible.
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
| Show All Characters | ✅     |      ✅     |      ⚠️[^2]    |    ⚠️[^2]  | ❌[^5] |
| Session Restore |     ✅     |      ✅     |      ✅        |    ✅      | ✅     |
| Hi-DPI Support |      ✅     |      ⚠️[^3] |      ✅        |    ⚠️[^3]  | ✅     |
| Dark Theme Support |  ✅     |      ✅     |      ❌        |    ❌      | ✅     |
| Side-by-Side View |   ❌[^6] |      ✅     |      ⚠️[^4]    |    ❌      | ✅     |
| Customizable Toolbar | ✅    |      ❌     |      ❌        |    ❌      | ✅     |
| Macro Recording |     ✅     |      ✅     |      ✅        |    ✅      | ❌     |
| Change History |      ✅     |      ✅     |      ❌        |    ✅      | ✅     |
| Smart Highlight [^9] | ✅    |      ✅     |      ✅        |    ✅      | ✅     |
| Editor Engine |    Scintilla  |   Scintilla |   Scintilla     | QScintilla | KTextEdit |

[^1]: Currently not open-sourced. Planned to be Open Source once Open Source Sponsor Goal is reached (please consider donate!).
[^2]: Some non-printable characters are not able to be shown.
[^3]: Some components have known issues with Hi-DPI support.
[^4]: Available as custom-tailored docking system.
[^5]: I created an add-on for this feature, but currently showing line ending is not possible to implement.
[^6]: Not planned in the initial release, might be added in the future.
[^9]: Highlight selected word in the editor.

## Open Source Goal

Pineapple Notepad is currently not an open source project, but the goal is to make Pineapple Notepad open source. I've set-up a $150 Ko-fi goal that Pineapple Notepad will be released as an Open-Source project if the goal is reached. Please read [this post](https://ko-fi.com/post/Pineapple-Notepad-A-Cross-platform-Notepad--Alte-G2G71V8JPX) for details. If you're interested, please use the following links to support this project.

### Sponsor Benefits

Before the goal is reached, if you support Pineapple Notepad for equial or more than $10, you can be credited within the application's About dialog as a kickstart supporter.

Please read the following details for sponsor benefits and possible restrictions.

<details>

All listed benefits below only apply to sponsors who donate **before** we reach the Open Source goal.

#### Tier 1: 10 USD

- Get pre-released builds of Pineapple Notepad.
- You (individual) will be credited within the application's About dialog as a kickstart supporter.
  - The name will be kept for at least 10 releases, after that, the name might be moved a standalone `THANKS` file in the source repository.
  - Optional email and/or home page could be provided.
  - Restriction applies (Please reach out if you have any questions or not sure if it applies to you):
    - The name, email and/or link to your home page should not contain political content, any emoji or other contents might cause me trouble.

#### Tier 2: 35 USD

- All benefits of Tier 1
- Get full source code of Pineapple Notepad that allows you build it from source or audit it.
- You (company/organization) will be credited within the application's About dialog as a sponsor.
  - The name will be kept for at least 10 releases, after that, the name might be moved a standalone `THANKS` file in the source repository.
  - Optional email and/or home page could be provided.
  - Restriction applies (Please reach out if you have any questions or not sure if it applies to you):
    - The name, email and/or link to your home page should not contain political content, any emoji or other contents might cause me trouble.
    - Gambling, blockchain, adult content and/or other similar contents are not allowed.

</details>

*If you have questions about sponsor benefits, please reach out to me by creating an issue or drop me a message on Ko-fi or any other form that can be reached.*

### Funding

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/blumia)

[![Afdian](https://static.afdiancdn.com/static/img/logo/logo.png)Afdian](https://afdian.com/a/BLumia)

## License

Pineapple Notepad is released under Pineapple Notepad End User License Agreement.

This section will be updated once the Open Source goal is reached.
