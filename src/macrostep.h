#pragma once

#include <cstddef>
#include <cstdint>

#include <ScintillaTypes.h>
#include <ScintillaMessages.h>

#include <QByteArray>
#include <QString>

class SciEdit;

class MacroStep
{
public:
    MacroStep() = default;
    MacroStep(Scintilla::Message message, Scintilla::uptr_t wParam, Scintilla::sptr_t lParam);

    void replay(SciEdit *editor) const;

    static bool messageHasString(Scintilla::Message message);
    static QString messageName(Scintilla::Message message);

    Scintilla::Message message{};
    Scintilla::uptr_t wParam = 0;
    Scintilla::sptr_t lParam = 0;
    QByteArray str;
};
