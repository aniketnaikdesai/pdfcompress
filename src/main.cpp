#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QFileDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QTextEdit>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <qpdf/QPDF.hh>

#include "core/PDFInspector.h"
#include "core/PDFOptimizer.h"
#include "DropHandler.h"
#include "DropOverlay.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow mainWindow;
    mainWindow.setWindowTitle("macOS PDF Compressor");
    mainWindow.resize(920, 750);

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
    
    auto *openButton = new QPushButton("Inspect PDF", centralWidget);
    openButton->setFixedHeight(36);
    buttonLayout->addWidget(openButton);

    auto *optimizeButton = new QPushButton("Optimize PDF", centralWidget);
    optimizeButton->setFixedHeight(36);
    buttonLayout->addWidget(optimizeButton);

    layout->addLayout(buttonLayout);

    // Profile & Quality controls
    auto *controlsGroup = new QGroupBox("Optimization Settings", centralWidget);
    auto *controlsLayout = new QVBoxLayout(controlsGroup);

    auto *profileLayout = new QHBoxLayout();
    auto *profileLabel = new QLabel("Profile:", controlsGroup);
    profileLabel->setFixedWidth(100);
    profileLayout->addWidget(profileLabel);

    auto *profileCombo = new QComboBox(controlsGroup);
    profileCombo->addItem("Balanced (Recommended)", static_cast<int>(pdfcompress::CompressionProfile::Balanced));
    profileCombo->addItem("Max Quality (Lossless & Safe)", static_cast<int>(pdfcompress::CompressionProfile::MaxQuality));
    profileCombo->addItem("Max Compression (Aggressive)", static_cast<int>(pdfcompress::CompressionProfile::MaxCompression));
    profileLayout->addWidget(profileCombo, 1);
    controlsLayout->addLayout(profileLayout);

    auto *qualityLayout = new QHBoxLayout();
    auto *qualityLabel = new QLabel("JPEG Quality:", controlsGroup);
    qualityLabel->setFixedWidth(100);
    qualityLayout->addWidget(qualityLabel);

    auto *qualitySlider = new QSlider(Qt::Horizontal, controlsGroup);
    qualitySlider->setRange(1, 100);
    qualitySlider->setValue(70);
    qualitySlider->setTickPosition(QSlider::TicksBelow);
    qualitySlider->setTickInterval(10);
    qualityLayout->addWidget(qualitySlider, 1);

    auto *qualityValueLabel = new QLabel("70", controlsGroup);
    qualityValueLabel->setFixedWidth(30);
    qualityLayout->addWidget(qualityValueLabel);
    controlsLayout->addLayout(qualityLayout);

    // Checkboxes layout
    auto *optionsLayout = new QGridLayout();
    auto *stripLinksCheck = new QCheckBox("Strip Links", controlsGroup);
    auto *stripAnnotsCheck = new QCheckBox("Strip Annotations", controlsGroup);
    auto *stripBookmarksCheck = new QCheckBox("Strip Bookmarks", controlsGroup);
    auto *stripFormsCheck = new QCheckBox("Strip Forms (AcroForm)", controlsGroup);
    auto *stripJsCheck = new QCheckBox("Strip JavaScript", controlsGroup);
    auto *stripDestsCheck = new QCheckBox("Strip Named Destinations", controlsGroup);
    auto *stripMetadataCheck = new QCheckBox("Strip Metadata (/Info, XMP)", controlsGroup);
    auto *linearizeCheck = new QCheckBox("Linearize (Fast Web View)", controlsGroup);

    optionsLayout->addWidget(stripMetadataCheck, 0, 0);
    optionsLayout->addWidget(stripJsCheck, 0, 1);
    optionsLayout->addWidget(stripBookmarksCheck, 0, 2);
    optionsLayout->addWidget(stripFormsCheck, 0, 3);
    optionsLayout->addWidget(stripLinksCheck, 1, 0);
    optionsLayout->addWidget(stripAnnotsCheck, 1, 1);
    optionsLayout->addWidget(stripDestsCheck, 1, 2);
    optionsLayout->addWidget(linearizeCheck, 1, 3);
    controlsLayout->addLayout(optionsLayout);

    layout->addWidget(controlsGroup);

    // Helper to sync checkboxes with selected profile
    auto applyProfileDefaults = [=](pdfcompress::CompressionProfile prof) {
        auto opts = pdfcompress::OptimizationOptions::forProfile(prof);
        stripLinksCheck->setChecked(opts.stripLinks);
        stripAnnotsCheck->setChecked(opts.stripOtherAnnotations);
        stripBookmarksCheck->setChecked(opts.stripBookmarks);
        stripFormsCheck->setChecked(opts.stripForms);
        stripJsCheck->setChecked(opts.stripJavaScript);
        stripDestsCheck->setChecked(opts.stripNamedDestinations);
        stripMetadataCheck->setChecked(opts.stripMetadata);
        linearizeCheck->setChecked(opts.linearize);
    };

    // Initialize defaults to Balanced
    applyProfileDefaults(pdfcompress::CompressionProfile::Balanced);

    QObject::connect(profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
        auto prof = static_cast<pdfcompress::CompressionProfile>(profileCombo->itemData(index).toInt());
        applyProfileDefaults(prof);
    });

    // Results area
    auto *resultsText = new QTextEdit(centralWidget);
    resultsText->setReadOnly(true);
    resultsText->setPlaceholderText("Select an action or drag & drop PDF files...");
    layout->addWidget(resultsText);

    // Dropped file paths
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
        auto prof = static_cast<pdfcompress::CompressionProfile>(profileCombo->currentData().toInt());
        
        pdfcompress::OptimizationOptions opts;
        opts.profile = prof;
        opts.qualityHint = quality;
        opts.stripLinks = stripLinksCheck->isChecked();
        opts.stripOtherAnnotations = stripAnnotsCheck->isChecked();
        opts.stripBookmarks = stripBookmarksCheck->isChecked();
        opts.stripForms = stripFormsCheck->isChecked();
        opts.stripJavaScript = stripJsCheck->isChecked();
        opts.stripNamedDestinations = stripDestsCheck->isChecked();
        opts.stripMetadata = stripMetadataCheck->isChecked();
        opts.linearize = linearizeCheck->isChecked();
        opts.recompressFlate = true;
        opts.deduplicateStreams = true;

        resultsText->append(QString("Optimizing %1 file(s)...\n").arg(files.size()));
        resultsText->append(QString("Profile: %1 | JPEG Quality: %2\n\n")
            .arg(profileCombo->currentText())
            .arg(quality));

        for (const auto& filePath : files) {
            resultsText->append(QString("--- %1 ---\n").arg(filePath));
            QApplication::processEvents();

            QString outPath = filePath;
            int lastDot = outPath.lastIndexOf('.');
            if (lastDot != -1) {
                outPath.insert(lastDot, "_optimized");
            } else {
                outPath += "_optimized.pdf";
            }

            try {
                pdfcompress::PDFOptimizer optimizer;
                auto result = optimizer.optimize(filePath.toStdString(), outPath.toStdString(), opts);

                if (result.success) {
                    resultsText->append(QString("  Output: %1").arg(outPath));
                    double savings = 100.0 * (1.0 - static_cast<double>(result.optimizedSizeBytes) / result.originalSizeBytes);
                    resultsText->append(QString("  Original: %1 KB  ->  Optimized: %2 KB (%3% reduction)")
                                            .arg(result.originalSizeBytes / 1024)
                                            .arg(result.optimizedSizeBytes / 1024)
                                            .arg(savings, 0, 'f', 1));
                    resultsText->append(QString("  Images: %1 processed (%2 optimized, %3 kept-original, %4 skipped)")
                                            .arg(result.imagesProcessed)
                                            .arg(result.imagesOptimized)
                                            .arg(result.imagesKeptOriginal)
                                            .arg(result.imagesSkipped));
                    if (result.imageBytesSaved > 0) {
                        resultsText->append(QString("  Image Bytes Saved: %1 KB").arg(result.imageBytesSaved / 1024));
                    }
                    if (result.streamsDeduplicated > 0) {
                        resultsText->append(QString("  Streams Deduplicated: %1").arg(result.streamsDeduplicated));
                    }
                    QStringList strippedList;
                    if (result.linksRemoved > 0) strippedList.append(QString("%1 links").arg(result.linksRemoved));
                    if (result.annotationsRemoved > 0) strippedList.append(QString("%1 annots").arg(result.annotationsRemoved));
                    if (result.bookmarksRemoved > 0) strippedList.append(QString("%1 bookmarks").arg(result.bookmarksRemoved));
                    if (result.formsRemoved > 0) strippedList.append(QString("%1 forms").arg(result.formsRemoved));
                    if (result.jsRemoved > 0) strippedList.append(QString("%1 JS").arg(result.jsRemoved));
                    if (result.namedDestinationsRemoved > 0) strippedList.append(QString("%1 dests").arg(result.namedDestinationsRemoved));
                    if (result.metadataStripped) strippedList.append("Metadata");

                    resultsText->append(QString("  Stripped: %1\n")
                                            .arg(strippedList.isEmpty() ? "None" : strippedList.join(", ")));
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
