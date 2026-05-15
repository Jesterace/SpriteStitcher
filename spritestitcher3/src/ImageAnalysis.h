#pragma once

#include <QImage>
#include <QRgb>
#include <QString>
#include <QVector>

struct ColorEntry {
    QRgb rgba = 0;
    int pixels = 0;
};

struct TransparencyOptions {
    bool treatBackgroundColorAsTransparent = false;
    bool hasBackgroundColor = false;
    QRgb backgroundColor = 0;
};

struct ImageAnalysisResult {
    bool ok = false;
    QString error;
    int width = 0;
    int height = 0;
    int transparentPixels = 0;
    int alphaTransparentPixels = 0;
    int backgroundTransparentPixels = 0;
    bool backgroundColorTransparencyEnabled = false;
    bool hasBackgroundColor = false;
    QRgb backgroundColor = 0;
    QVector<ColorEntry> colors;

    int uniqueColorCount() const;
};

namespace ImageAnalysis {
TransparencyOptions resolveTransparencyOptions(const QImage &source, const TransparencyOptions &options);
bool isNoStitchPixel(QRgb rgba, const TransparencyOptions &options);
QImage filteredImage(const QImage &source, const TransparencyOptions &options);
ImageAnalysisResult analyze(const QImage &source, const TransparencyOptions &options = TransparencyOptions());
ImageAnalysisResult analyzeFile(const QString &path, const TransparencyOptions &options = TransparencyOptions());
QString rgbaToHex(QRgb rgba);
QString rgbToHex(QRgb rgba);
}
