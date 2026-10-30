#pragma once

#include "macrostep.h"

#include <memory>
#include <QByteArray>
#include <QKeySequence>
#include <QVector>
#include <QString>

class SciEdit;

class Macro
{
public:
    void addStep(Scintilla::Message message, Scintilla::uptr_t wParam, Scintilla::sptr_t lParam);

    bool isEmpty() const { return m_steps.isEmpty(); }
    const QVector<MacroStep> &steps() const { return m_steps; }
    void replay(SciEdit *editor, int times = 1) const;
    void replayTillEndOfFile(SciEdit *editor) const;
    QByteArray serialize() const;
    static std::unique_ptr<Macro> deserialize(const QByteArray &bytes);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    QKeySequence shortcut() const { return m_shortcut; }
    void setShortcut(const QKeySequence &shortcut) { m_shortcut = shortcut; }

private:
    QString m_name;
    QKeySequence m_shortcut;
    QVector<MacroStep> m_steps;
};
