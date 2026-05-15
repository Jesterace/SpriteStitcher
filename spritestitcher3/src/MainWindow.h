#pragma once

#include "PatternModel.h"

#include <QImage>
#include <QMainWindow>

class QLabel;
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

private:
    void buildUi();
    void loadImage(const QString &path);
    void showPattern(const QString &path, const QImage &image, const PatternModel &model);
    void refreshChartPreview();
    void clearImage(const QString &message);

    QPushButton *m_openButton = nullptr;
    QPushButton *m_exportCsvButton = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_colorCountLabel = nullptr;
    QLabel *m_transparentCountLabel = nullptr;
    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_imageScrollArea = nullptr;
    QLabel *m_chartLabel = nullptr;
    QScrollArea *m_chartScrollArea = nullptr;
    QComboBox *m_chartZoomCombo = nullptr;
    QTableWidget *m_colorTable = nullptr;
    PatternModel m_patternModel;
    QString m_currentImagePath;
};
