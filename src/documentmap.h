#pragma once

#include <QDockWidget>

#include <cstdint>

class ScintillaEdit;
class SciEdit;
class ViewZone;
class DocumentManager;

class DocumentMap : public QDockWidget
{
    Q_OBJECT

public:
    explicit DocumentMap(DocumentManager *documentManager, QWidget *parent = nullptr);
    ~DocumentMap() override;

    void setActiveEditor(SciEdit *editor);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupMapView();
    void syncScroll();
    void syncWrapMode();
    void copyStylesFromEditor(SciEdit *editor);
    void onZoneClicked(int yPos);
    void onWheelScrolled(int delta);

    DocumentManager *m_documentManager;
    ScintillaEdit *m_mapView = nullptr;
    ViewZone *m_viewZone = nullptr;
    SciEdit *m_currentEditor = nullptr;
    intptr_t m_currentDocPointer = 0;
    bool m_initialDocReleased = false;
    int m_lastWrapMode = -1;
};
