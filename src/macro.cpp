#include "macro.h"

#include "sciedit.h"

#include <Scintilla.h>
#include <ScintillaMessages.h>

#include <QDataStream>
#include <QIODevice>

void Macro::addStep(Scintilla::Message message, Scintilla::uptr_t wParam, Scintilla::sptr_t lParam)
{
    if (message == Scintilla::Message::ReplaceSel && !m_steps.isEmpty() && m_steps.constLast().message == Scintilla::Message::ReplaceSel) {
        const char *p = reinterpret_cast<const char *>(lParam);
        if (p) {
            m_steps.last().str.append(QByteArray(p));
        }
        return;
    }

    if (message == Scintilla::Message::DeleteBack && !m_steps.isEmpty() && m_steps.constLast().message == Scintilla::Message::ReplaceSel) {
        if (m_steps.last().str.size() <= 1) {
            m_steps.removeLast();
        } else {
            m_steps.last().str.chop(1);
        }
        return;
    }

    m_steps.append(MacroStep(message, wParam, lParam));
}

void Macro::replay(SciEdit *editor, int times) const
{
    if (!editor || times <= 0) {
        return;
    }

    editor->setMacroPlayback(true);
    editor->beginUndoAction();
    for (int t = 0; t < times; ++t) {
        for (const MacroStep &step : m_steps) {
            step.replay(editor);
        }
    }
    editor->endUndoAction();
    editor->setMacroPlayback(false);
}

void Macro::replayTillEndOfFile(SciEdit *editor) const
{
    if (!editor || isEmpty()) {
        return;
    }

    editor->setMacroPlayback(true);
    editor->beginUndoAction();

    for (;;) {
        const sptr_t length = editor->length();
        const sptr_t curPos = editor->currentPos();

        for (const MacroStep &step : m_steps) {
            step.replay(editor);
        }

        if (editor->length() < length) {
            continue;
        } else if (editor->length() > length) {
            const sptr_t deltaLength = editor->length() - length;
            const sptr_t deltaPos = editor->currentPos() - curPos;
            if (deltaPos > deltaLength) {
                continue;
            }
        } else {
            if (editor->currentPos() != curPos) {
                continue;
            }
        }

        break;
    };

    editor->endUndoAction();
    editor->setMacroPlayback(false);
}

QByteArray Macro::serialize() const
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);

    out << QByteArray("PNpM03");
    out << m_name;
    out << m_shortcut.toString(QKeySequence::PortableText);
    out << static_cast<quint32>(m_steps.size());

    for (const MacroStep &step : m_steps) {
        const bool hasString = MacroStep::messageHasString(step.message);
        out << static_cast<qint32>(step.message);
        out << static_cast<quint64>(step.wParam);
        out << hasString;
        if (hasString) {
            out << step.str;
        } else {
            out << static_cast<qint64>(step.lParam);
        }
    }

    return bytes;
}

std::unique_ptr<Macro> Macro::deserialize(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return {};
    }

    QByteArray copy = bytes;
    QDataStream in(&copy, QIODevice::ReadOnly);
    QByteArray magic;
    in >> magic;

    auto readSteps = [&in]() -> QVector<MacroStep> {
        quint32 count = 0;
        in >> count;
        QVector<MacroStep> steps;
        steps.reserve(static_cast<int>(count));

        for (quint32 i = 0; i < count; ++i) {
            qint32 messageInt = 0;
            quint64 wParam64 = 0;
            bool hasString = false;
            in >> messageInt >> wParam64 >> hasString;

            MacroStep step;
            step.message = static_cast<Scintilla::Message>(messageInt);
            step.wParam = static_cast<Scintilla::uptr_t>(wParam64);
            if (hasString) {
                in >> step.str;
            } else {
                qint64 lParam64 = 0;
                in >> lParam64;
                step.lParam = static_cast<Scintilla::sptr_t>(lParam64);
            }

            steps.append(step);
        }
        return steps;
    };

    auto macro = std::make_unique<Macro>();

    if (magic == QByteArray("PNpM03")) {
        in >> macro->m_name;
        QString shortcutStr;
        in >> shortcutStr;
        macro->m_shortcut = QKeySequence(shortcutStr, QKeySequence::PortableText);
        macro->m_steps = readSteps();
    } else if (magic == QByteArray("PNpM02")) {
        in >> macro->m_name;
        macro->m_steps = readSteps();
    } else if (magic == QByteArray("PNpM01")) {
        macro->m_steps = readSteps();
    } else {
        return {};
    }

    if (in.status() != QDataStream::Ok) {
        return {};
    }

    if (macro->isEmpty()) {
        return {};
    }

    return macro;
}
