#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QFileDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QTextEdit>
#include <QMessageBox>
#include <qpdf/QPDF.hh>

#include "core/PDFInspector.h"
#include "core/PDFOptimizer.h"
#include "DropHandler.h"
#include "DropOverlay.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow mainWindow;
    mainWindow.setWindowTitle("macOS PDF Compressor");
    mainWindow.resize(900, 650);

    // Central widget
    auto *centralWidget = new QWidget(&mainWindow);
    auto *layout = new QVBoxLayout(centralWidget);

    // Version info
    QString versionInfo = QString("QPDF %1 | Qt %2 | PDFium loaded")
                              .arg(QPDF::QPDFVersion().c_str())
                              .arg(qVersion());
    auto *versionLabel = new QLabel(versionInfo, centralWidget);
    versionLabel->setStyleSheet("color: gray; font-size: 11px;");
    layout->addWidget(versionLabel);

    // Buttons layout
    auto *buttonLayout = new QHBoxLayout();
    
    // Open & Inspect button
    auto *openButton = new QPushButton("Inspect PDF", centralWidget);
    openButton->setFixedHeight(40);
    buttonLayout->addWidget(openButton);

    // Optimize button
    auto *optimizeButton = new QPushButton("Optimize PDF", centralWidget);
    optimizeButton->setFixedHeight(40);
    buttonLayout->addWidget(optimizeButton);

    layout->addLayout(buttonLayout);

    // Quality slider
    auto *qualityLayout = new QHBoxLayout();
    auto *qualityLabel = new QLabel("JPEG Quality:", centralWidget);
    qualityLabel->setFixedWidth(100);
    qualityLayout->addWidget(qualityLabel);

    auto *qualitySlider = new QSlider(Qt::Horizontal, centralWidget);
    qualitySlider->setRange(1, 100);
    qualitySlider->setValue(70);
    qualitySlider->setTickPosition(QSlider::TicksBelow);
    qualitySlider->setTickInterval(10);
    qualityLayout->addWidget(qualitySlider, 1);

    auto *qualityValueLabel = new QLabel("70", centralWidget);
    qualityValueLabel->setFixedWidth(30);
    qualityLayout->addWidget(qualityValueLabel);

    layout->addLayout(qualityLayout);

    // Results area
    auto *resultsText = new QTextEdit(centralWidget);
    resultsText->setReadOnly(true);
    resultsText->setPlaceholderText("Select an action...");
    layout->addWidget(resultsText);

    // Dropped file paths (filled by drag & drop, consumed by buttons)
    QStringList droppedFiles;

    // Connect Inspect button
    QObject::connect(openButton, &QPushButton::clicked, [&]() {
        QStringList files;
        if (!droppedFiles.isEmpty()) {
            files = droppedFiles;
            droppedFiles.clear();
        } else {
            QString f = QFileDialog::getOpenFileName(
                &mainWindow, "Select PDF", QString(), "PDF Files (*.pdf)");
            if (!f.isEmpty()) files.append(f);
        }

        if (files.isEmpty()) return;

        resultsText->clear();
        resultsText->append(QString("Inspecting %1 file(s)...\n").arg(files.size()));

        for (const auto& filePath : files) {
            resultsText->append(QString("\n--- %1 ---\n").arg(filePath));
            QApplication::processEvents();

            try {
                pdfcompress::PDFInspector inspector;
                auto info = inspector.inspect(filePath.toStdString(),
                    [&](int current, int total) {
                        resultsText->append(
                            QString("  Scanning page %1 / %2...")
                                .arg(current).arg(total));
                    });

                resultsText->append(QString("\n=== Document Info ==="));
                resultsText->append(QString("  PDF Version: %1").arg(info.pdfVersion.c_str()));
                resultsText->append(QString("  Pages: %1").arg(info.pageCount));
                resultsText->append(QString("  File Size: %1 KB").arg(info.fileSizeBytes / 1024));
                resultsText->append(QString("  Images Found: %1").arg(info.imageCount()));
                resultsText->append(QString("  Total Image Bytes: %1 KB")
                                        .arg(info.totalImageBytes() / 1024));

                resultsText->append(QString("\n=== Image Details ==="));
                for (const auto& img : info.images) {
                    resultsText->append(QString::fromStdString("  " + img.summary()));
                }

                resultsText->append("\nInspection complete.");

            } catch (const std::exception& e) {
                QMessageBox::critical(&mainWindow, "Error",
                                      QString("Failed to inspect %1:\n%2").arg(filePath, e.what()));
            }
        }
    });

    // Update quality label when slider moves
    QObject::connect(qualitySlider, &QSlider::valueChanged, [qualityValueLabel](int val) {
        qualityValueLabel->setText(QString::number(val));
    });

    // Connect Optimize button
    QObject::connect(optimizeButton, &QPushButton::clicked, [&]() {
        QStringList files;
        if (!droppedFiles.isEmpty()) {
            files = droppedFiles;
            droppedFiles.clear();
        } else {
            QString f = QFileDialog::getOpenFileName(
                &mainWindow, "Select PDF to Optimize", QString(), "PDF Files (*.pdf)");
            if (!f.isEmpty()) files.append(f);
        }

        if (files.isEmpty()) return;

        resultsText->clear();
        int quality = qualitySlider->value();
        resultsText->append(QString("Batch optimizing %1 file(s)...\n").arg(files.size()));
        resultsText->append(QString("JPEG Quality: %1\n\n").arg(quality));

        for (const auto& filePath : files) {
            resultsText->append(QString("--- %1 ---\n").arg(filePath));
            QApplication::processEvents();

            QString outPath = filePath;
            outPath.insert(outPath.lastIndexOf('.'), "_optimized");

            try {
                pdfcompress::PDFOptimizer optimizer;
                auto result = optimizer.optimize(filePath.toStdString(), outPath.toStdString(), pdfcompress::CompressionProfile::Balanced, quality);

                if (result.success) {
                    resultsText->append(QString("  Output: %1\n").arg(outPath));
                    resultsText->append(QString("  Original: %1 KB  Optimized: %2 KB")
                                            .arg(result.originalSizeBytes / 1024)
                                            .arg(result.optimizedSizeBytes / 1024));
                    
                    double savings = 100.0 * (1.0 - static_cast<double>(result.optimizedSizeBytes) / result.originalSizeBytes);
                    resultsText->append(QString("  Reduction: %1%").arg(savings, 0, 'f', 1));
                    
                    resultsText->append(QString("  Images: %1  Annotations: %2  Bookmarks: %3\n")
                                            .arg(result.imagesProcessed)
                                            .arg(result.annotationsRemoved)
                                            .arg(result.bookmarksRemoved));
                } else {
                    resultsText->append(QString("  FAILED: %1\n").arg(result.errorMessage.c_str()));
                }
            } catch (const std::exception& e) {
                QMessageBox::critical(&mainWindow, "Error",
                                      QString("Failed to optimize %1:\n%2").arg(filePath, e.what()));
            }
        }

        resultsText->append("\nBatch complete.");
    });

    // Drag & drop support
    auto* dropOverlay = new DropOverlay(centralWidget);
    auto* dropHandler = new DropHandler(&mainWindow, centralWidget, dropOverlay, &mainWindow);
    QObject::connect(dropHandler, &DropHandler::filesDropped,
        [resultsText, &droppedFiles](const QStringList& paths) {
            droppedFiles = paths;
            resultsText->clear();
            resultsText->append(QString("Dropped %1 file(s):").arg(paths.size()));
            for (const auto& path : paths) {
                resultsText->append("  " + path);
            }
            resultsText->append("\nClick Inspect or Optimize to process.");
        });

    mainWindow.setCentralWidget(centralWidget);
    mainWindow.show();

    return app.exec();
}
