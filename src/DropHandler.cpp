#include "DropHandler.h"
#include "DropOverlay.h"

#include <QMainWindow>
#include <QWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

DropHandler::DropHandler(QMainWindow* window, QWidget* contentWidget, DropOverlay* overlay, QObject* parent)
    : QObject(parent)
    , m_overlay(overlay)
{
    window->setAcceptDrops(true);
    window->installEventFilter(this);

    if (!contentWidget) return;

    contentWidget->setAcceptDrops(true);
    contentWidget->installEventFilter(this);

    // Install filter on every child so drag events are caught regardless
    // of which widget is under the cursor.
    for (auto* child : contentWidget->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively)) {
        child->installEventFilter(this);
    }

    if (m_overlay) {
        m_overlay->hide();
    }
}

bool DropHandler::eventFilter(QObject* obj, QEvent* event)
{
    switch (event->type()) {
    case QEvent::DragEnter: {
        auto* de = static_cast<QDragEnterEvent*>(event);
        if (de->mimeData() && de->mimeData()->hasUrls()) {
            de->acceptProposedAction();
            if (m_overlay) m_overlay->show();
            return true;
        }
        break;
    }
    case QEvent::DragMove: {
        static_cast<QDragMoveEvent*>(event)->acceptProposedAction();
        return true;
    }
    case QEvent::DragLeave: {
        if (m_overlay) m_overlay->hide();
        break;
    }
    case QEvent::Drop: {
        auto* drop = static_cast<QDropEvent*>(event);
        if (drop->mimeData() && drop->mimeData()->hasUrls()) {
            QStringList pdfPaths;
            for (const QUrl& url : drop->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    QString path = url.toLocalFile();
                    if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
                        pdfPaths.append(path);
                    }
                }
            }
            if (!pdfPaths.isEmpty()) {
                emit filesDropped(pdfPaths);
            }
        }
        if (m_overlay) m_overlay->hide();
        drop->acceptProposedAction();
        return true;
    }
    case QEvent::Resize: {
        if (m_overlay && obj == m_overlay->parentWidget()) {
            m_overlay->setGeometry(m_overlay->parentWidget()->rect());
        }
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}
