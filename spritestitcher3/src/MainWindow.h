#pragma once

#include "ImageAnalysis.h"

#include <QImage>
#include <QMainWindow>

class QLabel;
class QPushButton;
class QScrollArea;
class QTableWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openPng();

private:
    void buildUi();
    void loadImage(const QString &path);
    void showAnalysis(const QString &path, const QImage &image, const ImageAnalysisResult &analysis);
    void clearImage(const QString &message);

    QPushButton *m_openButton = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_colorCountLabel = nullptr;
    QLabel *m_transparentCountLabel = nullptr;
    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_imageScrollArea = nullptr;
    QTableWidget *m_colorTable = nullptr;
};
