#pragma once

#include <QObject>
#include <QStringList>

class QMainWindow;
class QWidget;
class DropOverlay;

class DropHandler : public QObject {
    Q_OBJECT
public:
    DropHandler(QMainWindow* window, QWidget* contentWidget, DropOverlay* overlay, QObject* parent = nullptr);

signals:
    void filesDropped(const QStringList& paths);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    DropOverlay* m_overlay;
};
