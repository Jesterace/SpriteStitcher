#pragma once

#include <QImage>
#include <QRgb>
#include <QString>
#include <QVector>

struct ColorEntry {
    QRgb rgba = 0;
    int pixels = 0;
};

struct ImageAnalysisResult {
    bool ok = false;
    QString error;
    int width = 0;
    int height = 0;
    int transparentPixels = 0;
    QVector<ColorEntry> colors;

    int uniqueColorCount() const;
};

namespace ImageAnalysis {
ImageAnalysisResult analyze(const QImage &source);
ImageAnalysisResult analyzeFile(const QString &path);
QString rgbaToHex(QRgb rgba);
}
