#include "searchresultview.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QPoint>

#include <Scintilla.h>

namespace {

// True when the key event stands for one of the clipboard/selection actions
// this read-only view handles itself: either the platform's standard binding
// (Ctrl+C on Windows/Linux, Cmd+C on macOS) or the Ctrl-based combination
// (which is what the window-wide KStandardActions and Scintilla's built-in
// key table use, even on macOS).
bool isClipboardShortcut(const QKeyEvent *keyEvent)
{
    if (keyEvent->matches(QKeySequence::Copy) || keyEvent->matches(QKeySequence::SelectAll)
        || keyEvent->matches(QKeySequence::Cut) || keyEvent->matches(QKeySequence::Paste)
        || keyEvent->matches(QKeySequence::Undo)) {
        return true;
    }
    return false;
}

} // namespace

SearchResultView::SearchResultView(QWidget *parent)
    : SciEdit(parent)
{
    // Read-only result view: no popup, no modification events, no undo.
    setReadOnly(true);
    usePopUp(SC_POPUP_NEVER);
    setModEventMask(0);
    setUndoCollection(false);
    emptyUndoBuffer();

    // Turn off editor behaviour that does not apply to search results. The
    // fold margin (margin 2, enabled by the SciEdit constructor) stays.
    setLineNumbersEnabled(false);
    setMarginWidthN(0, 0);
    setMarginWidthN(1, 0);
    setUrlScanEnabled(false);
    setSmartHighlightEnabled(false);
    setTagMatchHighlightEnabled(false);
    setSpellCheckEnabled(false);
}

bool SearchResultView::event(QEvent *event)
{
    // Claim the clipboard/selection keys while the view has focus. An
    // accepted ShortcutOverride makes Qt deliver a plain key press to this
    // widget instead of running the shortcut map, so the window-wide
    // KStandardAction equivalents (whose Copy/Cut/Paste/Undo targets are
    // always the main editor) stay out of the way, and KXmlGui's
    // shortcut-conflict warning does not fire.
    if (event->type() == QEvent::ShortcutOverride) {
        if (const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event); isClipboardShortcut(keyEvent)) {
            event->accept();
            return true;
        }
    }
    return SciEdit::event(event);
}

void SearchResultView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!event) {
        return;
    }

    const sptr_t position = positionFromPoint(event->pos().x(), event->pos().y());
    const int line = position >= 0 ? int(lineFromPosition(position)) : -1;
    emit contextMenuRequested(event->globalPos(), line);
    event->accept();
}
