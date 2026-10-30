#pragma once

#include <QObject>
#include <QPointer>

#include <memory>
#include <cstddef>
#include <cstdint>

#include <ScintillaTypes.h>
#include <ScintillaMessages.h>

class SciEdit;
class Macro;

class MacroRecorder : public QObject
{
public:
    explicit MacroRecorder(QObject *parent = nullptr);
    ~MacroRecorder() override;

    bool isRecording() const;

    void start(SciEdit *editor);
    std::unique_ptr<Macro> stop();

private:
    void recordStep(Scintilla::Message message, Scintilla::uptr_t wParam, Scintilla::sptr_t lParam);

    void clearEditorConnections();

    QPointer<SciEdit> m_editor;
    std::unique_ptr<Macro> m_macro;
};
