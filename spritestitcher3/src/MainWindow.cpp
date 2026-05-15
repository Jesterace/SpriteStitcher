#include "MainWindow.h"

#include "DmcMatcher.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
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
    m_pathLabel = new QLabel(QStringLiteral("No image loaded."), topBar);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    topLayout->addWidget(m_openButton);
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

    summaryLayout->addWidget(m_sizeLabel);
    summaryLayout->addWidget(m_colorCountLabel);
    summaryLayout->addWidget(m_transparentCountLabel);
    detailsLayout->addWidget(summaryBox);

    auto *colorsBox = new QGroupBox(QStringLiteral("Unique Colors"), detailsPanel);
    auto *colorsLayout = new QVBoxLayout(colorsBox);

    m_colorTable = new QTableWidget(0, 7, colorsBox);
    m_colorTable->setHorizontalHeaderLabels({
        QStringLiteral("Sprite"),
        QStringLiteral("Sprite RGBA"),
        QStringLiteral("Pixels"),
        QStringLiteral("DMC"),
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
    m_colorTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_colorTable->setAlternatingRowColors(true);

    colorsLayout->addWidget(m_colorTable);
    detailsLayout->addWidget(colorsBox, 1);

    splitter->addWidget(m_imageScrollArea);
    splitter->addWidget(detailsPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    setCentralWidget(central);
    resize(980, 640);

    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openPng);
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

void MainWindow::loadImage(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        clearImage(QStringLiteral("Unable to load image."));
        QMessageBox::warning(this, QStringLiteral("Open PNG"), reader.errorString());
        return;
    }

    const ImageAnalysisResult analysis = ImageAnalysis::analyze(image);
    if (!analysis.ok) {
        clearImage(analysis.error);
        QMessageBox::warning(this, QStringLiteral("Open PNG"), analysis.error);
        return;
    }

    showAnalysis(path, image, analysis);
}

void MainWindow::showAnalysis(const QString &path, const QImage &image, const ImageAnalysisResult &analysis) {
    m_pathLabel->setText(QFileInfo(path).absoluteFilePath());
    m_sizeLabel->setText(QStringLiteral("Size: %1 x %2 px").arg(analysis.width).arg(analysis.height));
    m_colorCountLabel->setText(QStringLiteral("Unique stitch colors: %1").arg(analysis.uniqueColorCount()));
    m_transparentCountLabel->setText(QStringLiteral("Transparent/background pixels: %1").arg(analysis.transparentPixels));

    const QPixmap pixmap = checkerboardPreview(image);
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setText(QString());
    m_imageLabel->setMinimumSize(pixmap.size());
    m_imageLabel->resize(pixmap.size());

    m_colorTable->setSortingEnabled(false);
    m_colorTable->setUpdatesEnabled(false);
    m_colorTable->clearContents();
    m_colorTable->setRowCount(analysis.colors.size());

    for (int row = 0; row < analysis.colors.size(); ++row) {
        const ColorEntry &entry = analysis.colors[row];
        const QColor color = QColor::fromRgba(entry.rgba);
        const DmcMatch match = DmcMatcher::nearest(entry.rgba);

        auto *swatch = new QTableWidgetItem;
        swatch->setBackground(QBrush(color));
        swatch->setText(color.alpha() < 255 ? QStringLiteral("alpha %1").arg(color.alpha()) : QString());

        auto *hex = new QTableWidgetItem(ImageAnalysis::rgbaToHex(entry.rgba));
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

        m_colorTable->setItem(row, 0, swatch);
        m_colorTable->setItem(row, 1, hex);
        m_colorTable->setItem(row, 2, pixels);
        m_colorTable->setItem(row, 3, dmcNumber);
        m_colorTable->setItem(row, 4, dmcName);
        m_colorTable->setItem(row, 5, dmcSwatch);
        m_colorTable->setItem(row, 6, distance);
    }

    m_colorTable->setUpdatesEnabled(true);
    m_colorTable->resizeColumnsToContents();
    m_colorTable->setSortingEnabled(true);
}

void MainWindow::clearImage(const QString &message) {
    m_pathLabel->setText(message);
    m_sizeLabel->setText(QStringLiteral("Size: -"));
    m_colorCountLabel->setText(QStringLiteral("Unique colors: -"));
    m_transparentCountLabel->setText(QStringLiteral("Transparent/background pixels: -"));
    m_colorTable->clearContents();
    m_colorTable->setRowCount(0);
    m_imageLabel->setPixmap(QPixmap());
    m_imageLabel->setText(message);
    m_imageLabel->setMinimumSize(360, 260);
    m_imageLabel->resize(360, 260);
}
