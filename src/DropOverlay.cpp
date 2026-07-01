#include "DropOverlay.h"
#include <QLabel>
#include <QVBoxLayout>

DropOverlay::DropOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setStyleSheet(
        "DropOverlay {"
        "  background: rgba(0, 122, 255, 0.08);"
        "  border: 5px dashed #007AFF;"
        "  border-radius: 10px;"
        "}"
    );

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto* iconLabel = new QLabel("\xE2\xAC\x87", this);
    iconLabel->setStyleSheet(
        "QLabel {"
        "  font-size: 48px;"
        "  background: transparent;"
        "  border: none;"
        "  color: #007AFF;"
        "}"
    );
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    auto* label = new QLabel("Drop PDF(s) here", this);
    label->setStyleSheet(
        "QLabel {"
        "  color: #007AFF;"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    setGeometry(parent ? parent->rect() : QRect());
    raise();
    hide();
}
