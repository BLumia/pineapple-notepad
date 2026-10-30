// SPDX-License-Identifier: MIT
// Per-editor spell check coordinator (see researches/spellcheck.md).
//
// Owns a Sonnet::Speller and implements the same scanning model as the URL
// scanner in SciEdit: debounce timer + visible-range-only clearing/filling of
// a dedicated Scintilla indicator (SciEdit::kSpellCheckIndicatorId, drawn as
// INDIC_SQUIGGLE). Which styles are checked is decided by SciEdit through
// SciEdit::styleNeedsSpellCheck() (per-style lookup table filled from the KSH
// lexer serialization "C" flag, the static BaseStyles tables, or plain-text
// mode when no lexer is set).

#pragma once

#include <ScintillaEdit.h>
#include <Sonnet/Speller>

#include <QObject>

class SciEdit;
class QTimer;

class SpellCheckController : public QObject
{
    Q_OBJECT
public:
    explicit SpellCheckController(SciEdit *editor, QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool enabled() const;

    // Rebuild the Speller from the current Sonnet configuration (e.g. after
    // the dictionary language changed in the settings dialog) and re-scan.
    void refreshSpeller();

    // Debounced visible-range scan request (scroll/resize/edit triggers).
    void requestScan();

    // Immediate visible-range scan: clears and re-fills the spell indicator
    // for the whole lines covered by the viewport.
    void scanNow();

    // Batch suspension while a document is being loaded (mirrors
    // SciEdit::beginUrlScanBatch()/endUrlScanBatch()).
    void suspendBatch();
    void resumeBatch();

    // Clear the indicators on the lines touched by an edit immediately (a
    // stale squiggle must not outlive the edit) and schedule a re-scan.
    void textModified(sptr_t position, sptr_t length);

    // Clear every spell indicator in the document.
    void clearAll();

    Sonnet::Speller &speller();

private:
    SciEdit *m_editor = nullptr;
    Sonnet::Speller m_speller;
    QTimer *m_debounceTimer = nullptr;
    int m_suspendCount = 0;
    bool m_enabled = false;
    bool m_forceOnResume = false;
};
