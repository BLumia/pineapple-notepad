#include "macrostep.h"

#include "sciedit.h"

#include <Scintilla.h>

MacroStep::MacroStep(Scintilla::Message message, Scintilla::uptr_t wParam, Scintilla::sptr_t lParam)
    : message(message)
    , wParam(wParam)
{
    if (messageHasString(message)) {
        if (message == Scintilla::Message::AddText || message == Scintilla::Message::AppendText) {
            str = QByteArray(reinterpret_cast<const char *>(lParam), static_cast<int>(wParam));
        } else {
            const char *p = reinterpret_cast<const char *>(lParam);
            str = p ? QByteArray(p) : QByteArray();
        }
    } else {
        this->lParam = lParam;
    }
}

void MacroStep::replay(SciEdit *editor) const
{
    if (!editor) {
        return;
    }

    if (messageHasString(message)) {
        editor->sends(static_cast<unsigned int>(message), wParam, str.constData());
        return;
    }

    editor->send(static_cast<unsigned int>(message), wParam, lParam);
}

bool MacroStep::messageHasString(Scintilla::Message message)
{
    return message == Scintilla::Message::ReplaceSel ||
        message == Scintilla::Message::InsertText ||
        message == Scintilla::Message::AddText ||
        message == Scintilla::Message::AppendText;
}

QString MacroStep::messageName(Scintilla::Message message)
{
    switch (message) {
    case Scintilla::Message::ReplaceSel: return QStringLiteral("Replace Selection");
    case Scintilla::Message::DeleteBack: return QStringLiteral("Delete Back");
    case Scintilla::Message::InsertText: return QStringLiteral("Insert Text");
    case Scintilla::Message::AddText: return QStringLiteral("Add Text");
    case Scintilla::Message::AppendText: return QStringLiteral("Append Text");
    case Scintilla::Message::Tab: return QStringLiteral("Tab");
    case Scintilla::Message::BackTab: return QStringLiteral("Back Tab");
    case Scintilla::Message::NewLine: return QStringLiteral("New Line");
    case Scintilla::Message::LineDelete: return QStringLiteral("Delete Line");
    case Scintilla::Message::LineDuplicate: return QStringLiteral("Duplicate Line");
    case Scintilla::Message::LowerCase: return QStringLiteral("Lower Case");
    case Scintilla::Message::UpperCase: return QStringLiteral("Upper Case");
    case Scintilla::Message::LineTranspose: return QStringLiteral("Transpose Line");
    case Scintilla::Message::DelWordLeft: return QStringLiteral("Delete Word Left");
    case Scintilla::Message::DelWordRight: return QStringLiteral("Delete Word Right");
    case Scintilla::Message::Home: return QStringLiteral("Home");
    case Scintilla::Message::LineEnd: return QStringLiteral("Line End");
    case Scintilla::Message::VCHome: return QStringLiteral("VC Home");
    case Scintilla::Message::LineCut: return QStringLiteral("Cut Line");
    case Scintilla::Message::LineCopy: return QStringLiteral("Copy Line");
    default: return QStringLiteral("Action (%1)").arg(static_cast<int>(message));
    }
}
