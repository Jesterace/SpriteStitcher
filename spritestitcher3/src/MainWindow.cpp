#include "MainWindow.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QFormLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QPixmap checkerboardPreview(const QImage &source) {
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    QImage canvas(image.size(), QImage::Format_RGB32);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const int square = 12;
    const QColor light(230, 230, 230);
    const QColor dark(190, 190, 190);
    for (int y = 0; y < canvas.height(); y += square) {
        for (int x = 0; x < canvas.width(); x += square) {
            const bool odd = ((x / square) + (y / square)) % 2;
            painter.fillRect(QRect(x, y, square, square), odd ? dark : light);
        }
    }

    painter.drawImage(0, 0, image);
    painter.end();
    return QPixmap::fromImage(canvas);
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SpriteStitcher 3 Prototype v%1").arg(APP_VERSION));
    buildUi();
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto *topBar = new QWidget(central);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);

    m_openButton = new QPushButton(QStringLiteral("Open PNG..."), topBar);
    m_exportCsvButton = new QPushButton(QStringLiteral("Export CSV..."), topBar);
    m_exportCsvButton->setEnabled(false);
    m_pathLabel = new QLabel(QStringLiteral("No image loaded."), topBar);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    topLayout->addWidget(m_openButton);
    topLayout->addWidget(m_exportCsvButton);
    topLayout->addWidget(m_pathLabel, 1);
    root->addWidget(topBar);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);

    m_imageLabel = new QLabel(QStringLiteral("Open a PNG sprite image."), splitter);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(360, 260);
    m_imageLabel->setFrameShape(QFrame::StyledPanel);
    m_imageLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_imageScrollArea = new QScrollArea(splitter);
    m_imageScrollArea->setWidget(m_imageLabel);
    m_imageScrollArea->setWidgetResizable(false);
    m_imageScrollArea->setAlignment(Qt::AlignCenter);

    auto *detailsPanel = new QWidget(splitter);
    detailsPanel->setMinimumWidth(300);
    auto *detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(8);

    auto *summaryBox = new QGroupBox(QStringLiteral("Image Summary"), detailsPanel);
    auto *summaryLayout = new QVBoxLayout(summaryBox);

    m_sizeLabel = new QLabel(QStringLiteral("Size: -"), summaryBox);
    m_sizeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_colorCountLabel = new QLabel(QStringLiteral("Unique colors: -"), summaryBox);
    m_colorCountLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_transparentCountLabel = new QLabel(QStringLiteral("Transparent/background pixels: -"), summaryBox);
    m_transparentCountLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_backgroundTransparentCheckBox = new QCheckBox(QStringLiteral("Treat background color as transparent"), summaryBox);

    auto *backgroundColorRow = new QWidget(summaryBox);
    auto *backgroundColorLayout = new QHBoxLayout(backgroundColorRow);
    backgroundColorLayout->setContentsMargins(0, 0, 0, 0);
    m_backgroundColorSwatch = new QLabel(backgroundColorRow);
    m_backgroundColorSwatch->setFixedSize(28, 18);
    m_backgroundColorSwatch->setFrameShape(QFrame::StyledPanel);
    m_backgroundColorSwatch->setAutoFillBackground(true);
    m_backgroundColorLabel = new QLabel(QStringLiteral("Background color: -"), backgroundColorRow);
    m_backgroundColorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    backgroundColorLayout->addWidget(m_backgroundColorSwatch);
    backgroundColorLayout->addWidget(m_backgroundColorLabel, 1);

    summaryLayout->addWidget(m_sizeLabel);
    summaryLayout->addWidget(m_colorCountLabel);
    summaryLayout->addWidget(m_transparentCountLabel);
    summaryLayout->addWidget(m_backgroundTransparentCheckBox);
    summaryLayout->addWidget(backgroundColorRow);
    detailsLayout->addWidget(summaryBox);

    auto *colorsBox = new QGroupBox(QStringLiteral("Unique Colors"), detailsPanel);
    auto *colorsLayout = new QVBoxLayout(colorsBox);

    auto *chartBox = new QGroupBox(QStringLiteral("Chart Preview"), detailsPanel);
    auto *chartLayout = new QVBoxLayout(chartBox);
    auto *chartControls = new QWidget(chartBox);
    auto *chartControlsLayout = new QHBoxLayout(chartControls);
    chartControlsLayout->setContentsMargins(0, 0, 0, 0);
    chartControlsLayout->addWidget(new QLabel(QStringLiteral("Zoom:"), chartControls));
    m_chartZoomCombo = new QComboBox(chartControls);
    m_chartZoomCombo->addItem(QStringLiteral("1x"), 12);
    m_chartZoomCombo->addItem(QStringLiteral("2x"), 18);
    m_chartZoomCombo->addItem(QStringLiteral("3x"), 24);
    m_chartZoomCombo->addItem(QStringLiteral("4x"), 32);
    chartControlsLayout->addWidget(m_chartZoomCombo);
    chartControlsLayout->addStretch(1);
    chartLayout->addWidget(chartControls);

    m_chartLabel = new QLabel(QStringLiteral("Open a sprite to preview the chart."), chartBox);
    m_chartLabel->setAlignment(Qt::AlignCenter);
    m_chartLabel->setMinimumSize(360, 260);
    m_chartLabel->setFrameShape(QFrame::StyledPanel);
    m_chartLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_chartScrollArea = new QScrollArea(chartBox);
    m_chartScrollArea->setWidget(m_chartLabel);
    m_chartScrollArea->setWidgetResizable(false);
    m_chartScrollArea->setAlignment(Qt::AlignCenter);
    m_chartScrollArea->setFrameShape(QFrame::NoFrame);
    m_chartScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_chartScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    chartLayout->addWidget(m_chartScrollArea, 1);

    m_colorTable = new QTableWidget(0, 8, colorsBox);
    m_colorTable->setHorizontalHeaderLabels({
        QStringLiteral("Symbol"),
        QStringLiteral("Sprite"),
        QStringLiteral("Sprite RGBA"),
        QStringLiteral("Pixels"),
        QStringLiteral("DMC Code"),
        QStringLiteral("DMC Name"),
        QStringLiteral("DMC Swatch"),
        QStringLiteral("Distance^2")
    });
    m_colorTable->verticalHeader()->setVisible(false);
    m_colorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_colorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_colorTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_colorTable->horizontalHeader()->setStretchLastSection(true);
    m_colorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_colorTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_colorTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_colorTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_colorTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_colorTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_colorTable->setAlternatingRowColors(true);

    colorsLayout->addWidget(m_colorTable);
    detailsLayout->addWidget(chartBox, 1);
    detailsLayout->addWidget(colorsBox, 1);

    splitter->addWidget(m_imageScrollArea);
    splitter->addWidget(detailsPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    setCentralWidget(central);
    resize(980, 640);

    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openPng);
    connect(m_exportCsvButton, &QPushButton::clicked, this, &MainWindow::exportCsv);
    connect(m_chartZoomCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { refreshChartPreview(); });
    connect(m_backgroundTransparentCheckBox, &QCheckBox::toggled, this, [this] { rebuildPattern(); });
    updateBackgroundColorDisplay(QImage());
}

void MainWindow::openPng() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open PNG Sprite"),
        QString(),
        QStringLiteral("PNG images (*.png)"));

    if (!path.isEmpty()) {
        loadImage(path);
    }
}

void MainWindow::exportCsv() {
    if (!m_patternModel.ok) {
        QMessageBox::warning(this, QStringLiteral("Export CSV"), QStringLiteral("Open a PNG before exporting CSV."));
        return;
    }

    QString defaultPath = QStringLiteral("spritestitcher3_palette.csv");
    if (!m_currentImagePath.isEmpty()) {
        const QFileInfo info(m_currentImagePath);
        defaultPath = info.dir().filePath(info.completeBaseName() + QStringLiteral("_spritestitcher3.csv"));
    }

    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export CSV"),
        defaultPath,
        QStringLiteral("CSV files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".csv");
    }

    QString error;
    if (!m_patternModel.writeCsvFile(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Export CSV"), error);
        return;
    }

    QMessageBox::information(this, QStringLiteral("Export CSV"), QStringLiteral("CSV exported."));
}

void MainWindow::loadImage(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        clearImage(QStringLiteral("Unable to load image."));
        QMessageBox::warning(this, QStringLiteral("Open PNG"), reader.errorString());
        return;
    }

    m_sourceImage = image;
    m_currentImagePath = QFileInfo(path).absoluteFilePath();
    rebuildPattern();
}

void MainWindow::rebuildPattern() {
    if (m_sourceImage.isNull()) {
        updateBackgroundColorDisplay(QImage());
        return;
    }

    const TransparencyOptions options = currentTransparencyOptions();
    const PatternModel model = PatternModel::fromImage(m_sourceImage, options);
    if (!model.ok) {
        clearImage(model.error);
        QMessageBox::warning(this, QStringLiteral("Open PNG"), model.error);
        return;
    }

    showPattern(m_currentImagePath, ImageAnalysis::filteredImage(m_sourceImage, options), model);
}

TransparencyOptions MainWindow::currentTransparencyOptions() const {
    TransparencyOptions options;
    options.treatBackgroundColorAsTransparent = m_backgroundTransparentCheckBox &&
        m_backgroundTransparentCheckBox->isChecked();
    return options;
}

void MainWindow::showPattern(const QString &path, const QImage &image, const PatternModel &model) {
    m_patternModel = model;
    m_currentImagePath = QFileInfo(path).absoluteFilePath();
    m_exportCsvButton->setEnabled(true);

    m_pathLabel->setText(m_currentImagePath);
    m_sizeLabel->setText(QStringLiteral("Size: %1 x %2 px").arg(model.imageWidth).arg(model.imageHeight));
    m_colorCountLabel->setText(QStringLiteral("Unique stitch colors: %1, matched DMC colors: %2")
                                   .arg(model.uniqueSpriteColorCount())
                                   .arg(model.matchedColorCount()));
    m_transparentCountLabel->setText(QStringLiteral("Transparent/background pixels: %1").arg(model.transparentPixels));
    updateBackgroundColorDisplay(m_sourceImage);

    const QPixmap pixmap = checkerboardPreview(image);
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setText(QString());
    m_imageLabel->setMinimumSize(pixmap.size());
    m_imageLabel->resize(pixmap.size());

    refreshChartPreview();

    m_colorTable->setSortingEnabled(false);
    m_colorTable->setUpdatesEnabled(false);
    m_colorTable->clearContents();
    m_colorTable->setRowCount(model.spriteColors.size());

    for (int row = 0; row < model.spriteColors.size(); ++row) {
        const PatternSpriteColor &entry = model.spriteColors[row];
        const QColor color = QColor::fromRgba(entry.rgba);
        const DmcMatch &match = entry.dmcMatch;
        const QString symbol = (entry.matchedColorIndex >= 0 && entry.matchedColorIndex < model.matchedColors.size())
            ? model.matchedColors[entry.matchedColorIndex].symbol
            : QString();

        auto *symbolItem = new QTableWidgetItem(symbol);
        auto *swatch = new QTableWidgetItem;
        swatch->setBackground(QBrush(color));
        swatch->setText(color.alpha() < 255 ? QStringLiteral("alpha %1").arg(color.alpha()) : QString());

        auto *hex = new QTableWidgetItem(entry.hex);
        auto *pixels = new QTableWidgetItem;
        pixels->setData(Qt::DisplayRole, entry.pixels);
        auto *dmcNumber = new QTableWidgetItem(match.ok ? match.color.number : QString());
        auto *dmcName = new QTableWidgetItem(match.ok ? match.color.name : QString());
        auto *dmcSwatch = new QTableWidgetItem;
        if (match.ok) {
            dmcSwatch->setBackground(QBrush(match.color.color));
        }
        auto *distance = new QTableWidgetItem;
        distance->setData(Qt::DisplayRole, match.ok ? match.distanceSquared : 0);

        m_colorTable->setItem(row, 0, symbolItem);
        m_colorTable->setItem(row, 1, swatch);
        m_colorTable->setItem(row, 2, hex);
        m_colorTable->setItem(row, 3, pixels);
        m_colorTable->setItem(row, 4, dmcNumber);
        m_colorTable->setItem(row, 5, dmcName);
        m_colorTable->setItem(row, 6, dmcSwatch);
        m_colorTable->setItem(row, 7, distance);
    }

    m_colorTable->setUpdatesEnabled(true);
    m_colorTable->resizeColumnsToContents();
    m_colorTable->setSortingEnabled(true);
}

void MainWindow::updateBackgroundColorDisplay(const QImage &image) {
    if (!m_backgroundColorLabel || !m_backgroundColorSwatch) {
        return;
    }

    QColor color(240, 240, 240);
    if (image.isNull()) {
        m_backgroundColorLabel->setText(QStringLiteral("Background color: -"));
    } else {
        const QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);
        const QRgb backgroundColor = argbImage.pixel(0, 0);
        const QString suffix = (m_backgroundTransparentCheckBox && m_backgroundTransparentCheckBox->isChecked())
            ? QString()
            : QStringLiteral(" (not applied)");
        m_backgroundColorLabel->setText(QStringLiteral("Background color: %1%2")
            .arg(ImageAnalysis::rgbToHex(backgroundColor), suffix));
        color = QColor(qRed(backgroundColor), qGreen(backgroundColor), qBlue(backgroundColor));
    }

    QPalette palette = m_backgroundColorSwatch->palette();
    palette.setColor(QPalette::Window, color);
    m_backgroundColorSwatch->setPalette(palette);
}

void MainWindow::refreshChartPreview() {
    if (!m_chartLabel || !m_chartScrollArea) return;
    if (!m_patternModel.ok || m_patternModel.imageWidth <= 0 || m_patternModel.imageHeight <= 0) {
        m_chartLabel->setPixmap(QPixmap());
        m_chartLabel->setText(QStringLiteral("Open a sprite to preview the chart."));
        m_chartLabel->setMinimumSize(360, 260);
        m_chartLabel->resize(360, 260);
        return;
    }

    const int cellSize = m_chartZoomCombo ? m_chartZoomCombo->currentData().toInt() : 12;
    const QImage chart = m_patternModel.renderChartPreview(cellSize, true);
    if (chart.isNull()) return;

    const QPixmap pixmap = QPixmap::fromImage(chart);
    m_chartLabel->setPixmap(pixmap);
    m_chartLabel->setText(QString());
    m_chartLabel->setMinimumSize(pixmap.size());
    m_chartLabel->resize(pixmap.size());
}

void MainWindow::clearImage(const QString &message) {
    m_patternModel = PatternModel();
    m_sourceImage = QImage();
    m_currentImagePath.clear();
    m_exportCsvButton->setEnabled(false);
    m_pathLabel->setText(message);
    m_sizeLabel->setText(QStringLiteral("Size: -"));
    m_colorCountLabel->setText(QStringLiteral("Unique colors: -"));
    m_transparentCountLabel->setText(QStringLiteral("Transparent/background pixels: -"));
    m_colorTable->clearContents();
    m_colorTable->setRowCount(0);
    updateBackgroundColorDisplay(QImage());
    m_imageLabel->setPixmap(QPixmap());
    m_imageLabel->setText(message);
    m_imageLabel->setMinimumSize(360, 260);
    m_imageLabel->resize(360, 260);
    refreshChartPreview();
}
