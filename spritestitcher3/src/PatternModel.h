#pragma once

#include "DmcMatcher.h"
#include "ImageAnalysis.h"

#include <QImage>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>

struct PatternSpriteColor {
    QRgb rgba = 0;
    QString hex;
    int pixels = 0;
    DmcMatch dmcMatch;
    int matchedColorIndex = -1;
};

struct PatternMatchedColor {
    QString symbol;
    DmcColor dmc;
    int stitchCount = 0;
    QVector<int> spriteColorIndexes;
    QStringList sourceSpriteColorHexes;

    int sourceColorCount() const;
};

struct PatternPixel {
    int x = 0;
    int y = 0;
    int spriteColorIndex = -1;
    int matchedColorIndex = -1;
};

enum class ChartMode {
    ColorAndSymbols,
    SymbolsOnly,
    ColorsOnly
};

class PatternModel {
public:
    bool ok = false;
    QString error;
    int imageWidth = 0;
    int imageHeight = 0;
    int opaqueStitchPixels = 0;
    int transparentPixels = 0;
    int alphaTransparentPixels = 0;
    int backgroundTransparentPixels = 0;
    bool backgroundColorTransparencyEnabled = false;
    bool hasBackgroundColor = false;
    QRgb backgroundColor = 0;
    QVector<PatternPixel> stitchPixels;
    QVector<QPoint> noStitchPixels;
    QVector<int> stitchGrid;
    QVector<PatternSpriteColor> spriteColors;
    QVector<PatternMatchedColor> matchedColors;

    static PatternModel fromImage(const QImage &image, const TransparencyOptions &options = TransparencyOptions());
    static PatternModel fromAnalysis(const ImageAnalysisResult &analysis);

    int uniqueSpriteColorCount() const;
    int matchedColorCount() const;
    QString finishedSizeText(int fabricCount) const;
    QImage renderChartPreview(int cellSize = 16, bool drawCenterLines = true, ChartMode chartMode = ChartMode::ColorAndSymbols) const;
    bool writeChartPngFile(const QString &path, int cellSize = 16, QString *errorMessage = nullptr, ChartMode chartMode = ChartMode::ColorAndSymbols) const;
    bool writePdfFile(const QString &path, const QString &imageName, int chartCellSize = 16, QString *errorMessage = nullptr, ChartMode chartMode = ChartMode::ColorAndSymbols) const;
    QString toCsv() const;
    bool writeCsvFile(const QString &path, QString *errorMessage = nullptr) const;

private:
    static QString symbolForIndex(int index);
};
