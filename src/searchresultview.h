#pragma once

#include "sciedit.h"

class QPoint;

// A trimmed-down SciEdit for hosting search results. Inherits SciEdit so the
// editor theme, font, folding helpers and marker machinery keep working, but
// disables the editing-oriented extras (context menu, URL scan, smart
// highlight, ...) that make no sense in a read-only results view.
class SearchResultView : public SciEdit
{
    Q_OBJECT

public:
    explicit SearchResultView(QWidget *parent = nullptr);

signals:
    // Emitted instead of the editor context menu; `line` is the buffer line
    // under the cursor, or -1 when the click is not over text.
    void contextMenuRequested(const QPoint &globalPos, int line);

protected:
    bool event(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};
