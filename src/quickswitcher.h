#pragma once

#include <QTreeView>

class DocumentManager;
class RecentTagProxyModel;

class QuickSwitcher : public QTreeView
{
    Q_OBJECT

public:
    QuickSwitcher(DocumentManager *dm, QWidget *parent = nullptr);

    void popup();
    void step(int offset);

signals:
    void documentActivated(int docId);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void activateCurrent();

    DocumentManager *m_dm;
    RecentTagProxyModel *m_proxy;
};
