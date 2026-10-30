#include "documentmap.h"
#include "viewzone.h"
#include "sciedit.h"
#include "documentmanager.h"

#include <ScintillaEdit.h>

DocumentMap::DocumentMap(DocumentManager *documentManager, QWidget *parent)
    : QDockWidget(tr("Document Map"), parent)
    , m_documentManager(documentManager)
{
    m_mapView = new ScintillaEdit(this);
    setWidget(m_mapView);

    setupMapView();

    m_viewZone = new ViewZone(m_mapView->viewport());
    m_viewZone->raise();

    connect(m_viewZone, &ViewZone::zoneClicked, this, &DocumentMap::onZoneClicked);
    connect(m_viewZone, &ViewZone::wheelScrolled, this, &DocumentMap::onWheelScrolled);
}

DocumentMap::~DocumentMap()
{
    if (m_currentEditor && m_currentDocPointer) {
        m_mapView->releaseDocument(m_currentDocPointer);
    }
}

void DocumentMap::setupMapView()
{
    m_mapView->setZoom(-10);
    m_mapView->setVScrollBar(false);
    m_mapView->setHScrollBar(false);
    m_mapView->setReadOnly(true);
    m_mapView->setScrollWidth(1);
    m_mapView->setScrollWidthTracking(false);
    m_mapView->setEndAtLastLine(true);
    m_mapView->setCaretLineVisible(false);

    for (int i = 0; i <= 4; i++) {
        m_mapView->setMarginWidthN(i, 0);
        m_mapView->setMarginSensitiveN(i, false);
    }

    m_mapView->setModEventMask(SC_MOD_NONE);
}

void DocumentMap::setActiveEditor(SciEdit *editor)
{
    if (m_currentEditor == editor) {
        return;
    }

    if (m_currentEditor) {
        disconnect(m_currentEditor, nullptr, this, nullptr);
    }

    m_currentEditor = editor;

    m_lastWrapMode = -1;

    if (!editor) {
        if (m_currentDocPointer) {
            m_mapView->releaseDocument(m_currentDocPointer);
            m_currentDocPointer = 0;
        }
        m_viewZone->setZone(0, 0);
        return;
    }

    intptr_t newDocPtr = editor->docPointer();
    if (newDocPtr != m_currentDocPointer) {
        if (m_currentDocPointer) {
            m_mapView->releaseDocument(m_currentDocPointer);
        }
        editor->addRefDocument(newDocPtr);
        m_mapView->setDocPointer(newDocPtr);
        m_currentDocPointer = newDocPtr;
    }

    copyStylesFromEditor(editor);
    m_mapView->setZoom(-10);

    connect(editor, &ScintillaEditBase::painted, this, [this]() {
        syncScroll();
    });

    syncScroll();
}

void DocumentMap::copyStylesFromEditor(SciEdit *editor)
{
    if (!editor || !m_mapView) {
        return;
    }

    for (int i = 0; i <= STYLE_MAX; i++) {
        m_mapView->styleSetFore(i, editor->styleFore(i));
        m_mapView->styleSetBack(i, editor->styleBack(i));
        m_mapView->styleSetBold(i, editor->styleBold(i));
        m_mapView->styleSetItalic(i, editor->styleItalic(i));
        m_mapView->styleSetUnderline(i, editor->styleUnderline(i));
    }
}

void DocumentMap::syncWrapMode()
{
    if (!m_currentEditor || !m_mapView) {
        return;
    }

    const sptr_t editorWrapMode = m_currentEditor->wrapMode();

    if (static_cast<int>(editorWrapMode) == m_lastWrapMode) {
        return;
    }

    m_lastWrapMode = static_cast<int>(editorWrapMode);

    if (editorWrapMode != SC_WRAP_NONE) {
        m_mapView->setWrapMode(editorWrapMode);
        m_mapView->setWrapIndentMode(m_currentEditor->wrapIndentMode());
    } else {
        m_mapView->setWrapMode(SC_WRAP_NONE);
    }
}

void DocumentMap::syncScroll()
{
    if (!m_currentEditor || !m_mapView) {
        return;
    }

    syncWrapMode();

    m_viewZone->resize(m_mapView->viewport()->size());

    const int editorWidth = m_currentEditor->viewport()->width();
    const int editorHeight = m_currentEditor->viewport()->height();
    if (editorWidth <= 0 || editorHeight <= 0) {
        return;
    }

    const sptr_t higherPos = m_currentEditor->positionFromPoint(0, 0);
    const sptr_t lowerPos = m_currentEditor->positionFromPoint(editorWidth, editorHeight);

    m_mapView->gotoPos(higherPos);
    m_mapView->gotoPos(lowerPos);

    const sptr_t higherY = m_mapView->pointYFromPosition(higherPos);
    const sptr_t lineHeightMap = m_mapView->textHeight(0);
    if (lineHeightMap <= 0) {
        return;
    }

    sptr_t lowerY;
    if (m_currentEditor->wrapMode() != SC_WRAP_NONE) {
        lowerY = m_mapView->pointYFromPosition(lowerPos) + lineHeightMap;
    } else {
        const sptr_t lineHeightEditor = m_currentEditor->textHeight(0);
        if (lineHeightEditor <= 0) {
            return;
        }
        lowerY = higherY + lineHeightMap * editorHeight / lineHeightEditor;
    }

    m_viewZone->setZone(static_cast<int>(higherY), static_cast<int>(lowerY));
}

void DocumentMap::onZoneClicked(int yPos)
{
    if (!m_currentEditor || !m_mapView) {
        return;
    }

    const int currentCenterY = m_viewZone->zoneCenter();
    const int pixelDistance = yPos - currentCenterY;
    const sptr_t lineHeightMap = m_mapView->textHeight(0);
    if (lineHeightMap <= 0) {
        return;
    }
    const int linesToJump = static_cast<int>(pixelDistance / lineHeightMap);

    m_currentEditor->lineScroll(0, linesToJump);
    syncScroll();
}

void DocumentMap::onWheelScrolled(int delta)
{
    if (!m_currentEditor) {
        return;
    }

    const int lines = (delta > 0) ? -3 : 3;
    m_currentEditor->lineScroll(0, lines);
}

void DocumentMap::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    syncScroll();
}
