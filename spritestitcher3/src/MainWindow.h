#pragma once

#include "PatternModel.h"

#include <QImage>
#include <QMainWindow>

class QLabel;
class QCheckBox;
class QComboBox;
class QPushButton;
class QScrollArea;
class QTableWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openPng();
    void exportCsv();
    void exportChartPng();
    void exportPdf();

private:
    void buildUi();
    void loadImage(const QString &path);
    void rebuildPattern();
    TransparencyOptions currentTransparencyOptions() const;
    void showPattern(const QString &path, const QImage &image, const PatternModel &model);
    void updateBackgroundColorDisplay(const QImage &image);
    int currentChartCellSize() const;
    void refreshChartPreview();
    void clearImage(const QString &message);

    QPushButton *m_openButton = nullptr;
    QPushButton *m_exportCsvButton = nullptr;
    QPushButton *m_exportChartPngButton = nullptr;
    QPushButton *m_exportPdfButton = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_colorCountLabel = nullptr;
    QLabel *m_transparentCountLabel = nullptr;
    QCheckBox *m_backgroundTransparentCheckBox = nullptr;
    QLabel *m_backgroundColorLabel = nullptr;
    QLabel *m_backgroundColorSwatch = nullptr;
    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_imageScrollArea = nullptr;
    QLabel *m_chartLabel = nullptr;
    QScrollArea *m_chartScrollArea = nullptr;
    QComboBox *m_chartZoomCombo = nullptr;
    QTableWidget *m_colorTable = nullptr;
    PatternModel m_patternModel;
    QImage m_sourceImage;
    QString m_currentImagePath;
};
