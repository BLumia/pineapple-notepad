#include "macrorecorder.h"

#include "macro.h"
#include "sciedit.h"

#include <Scintilla.h>

MacroRecorder::MacroRecorder(QObject *parent)
    : QObject(parent)
{
}

MacroRecorder::~MacroRecorder() = default;

bool MacroRecorder::isRecording() const
{
    return m_editor != nullptr;
}

void MacroRecorder::start(SciEdit *editor)
{
    if (!editor || isRecording()) {
        return;
    }

    m_editor = editor;
    m_macro = std::make_unique<Macro>();

    connect(editor, &ScintillaEditBase::macroRecord, this, &MacroRecorder::recordStep);
    connect(editor, &QObject::destroyed, this, [this]() {
        m_editor = nullptr;
        m_macro.reset();
    });

    editor->startRecord();
}

std::unique_ptr<Macro> MacroRecorder::stop()
{
    if (!isRecording()) {
        return {};
    }

    SciEdit *editor = m_editor.data();
    if (editor) {
        editor->stopRecord();
    }

    clearEditorConnections();

    return std::move(m_macro);
}

void MacroRecorder::recordStep(Scintilla::Message message, Scintilla::uptr_t wParam, Scintilla::sptr_t lParam)
{
    if (!m_macro) {
        return;
    }

    m_macro->addStep(message, wParam, lParam);
}

void MacroRecorder::clearEditorConnections()
{
    if (!m_editor) {
        return;
    }

    disconnect(m_editor.data(), nullptr, this, nullptr);
    m_editor = nullptr;
}
