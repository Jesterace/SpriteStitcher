#include "ImageAnalysis.h"

#include "DmcMatcher.h"
#include "PatternModel.h"

#include <QGuiApplication>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
#include <cstdio>

namespace {
int fail(const QString &message) {
    qWarning().noquote() << message;
    std::fprintf(stderr, "%s\n", message.toLocal8Bit().constData());
    return 1;
}

const ColorEntry *findColor(const QVector<ColorEntry> &colors, QRgb rgba) {
    for (const ColorEntry &entry : colors) {
        if (entry.rgba == rgba) return &entry;
    }
    return nullptr;
}

const PatternSpriteColor *findSpriteColor(const PatternModel &model, QRgb rgba) {
    for (const PatternSpriteColor &entry : model.spriteColors) {
        if (entry.rgba == rgba) return &entry;
    }
    return nullptr;
}

bool cellContainsInkOutsideColors(const QImage &image, const QRect &cell, const QVector<QColor> &allowedColors) {
    for (int y = cell.top(); y <= cell.bottom(); ++y) {
        for (int x = cell.left(); x <= cell.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            bool allowed = false;
            for (const QColor &color : allowedColors) {
                if (pixel == color) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                return true;
            }
        }
    }
    return false;
}

bool cellIsSolidColor(const QImage &image, const QRect &cell, const QColor &color) {
    for (int y = cell.top(); y <= cell.bottom(); ++y) {
        for (int x = cell.left(); x <= cell.right(); ++x) {
            if (image.pixelColor(x, y) != color) {
                return false;
            }
        }
    }
    return true;
}

bool cellContainsHighContrastSymbolPixel(const QImage &image, const QRect &cell) {
    for (int y = cell.top(); y <= cell.bottom(); ++y) {
        for (int x = cell.left(); x <= cell.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            const bool light = pixel.red() >= 245 && pixel.green() >= 245 && pixel.blue() >= 245;
            const bool dark = pixel.red() <= 40 && pixel.green() <= 40 && pixel.blue() <= 40;
            if (light || dark) {
                return true;
            }
        }
    }
    return false;
}

bool cellPixelsDiffer(const QImage &left, const QImage &right, const QRect &cell) {
    for (int y = cell.top(); y <= cell.bottom(); ++y) {
        for (int x = cell.left(); x <= cell.right(); ++x) {
            if (left.pixelColor(x, y) != right.pixelColor(x, y)) {
                return true;
            }
        }
    }
    return false;
}

int lightBackingPixelCount(const QImage &image, const QRect &cell) {
    int count = 0;
    for (int y = cell.top(); y <= cell.bottom(); ++y) {
        for (int x = cell.left(); x <= cell.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() >= 235 && pixel.green() >= 180 && pixel.blue() >= 180) {
                ++count;
            }
        }
    }
    return count;
}

bool isPdfRedChartPixel(const QColor &pixel) {
    return pixel.red() >= 150 &&
           pixel.green() <= 120 &&
           pixel.blue() <= 140 &&
           pixel.red() > pixel.green() + 50;
}

bool isPdfLightPixel(const QColor &pixel) {
    return pixel.red() >= 230 && pixel.green() >= 200 && pixel.blue() >= 200;
}

bool isPdfDarkPixel(const QColor &pixel) {
    return pixel.red() <= 100 && pixel.green() <= 100 && pixel.blue() <= 100;
}

bool pageContainsColorChartSymbolOverlay(const QImage &page) {
    if (page.isNull()) {
        return false;
    }

    const QRect searchArea(
        0,
        page.height() / 6,
        page.width() * 2 / 3,
        page.height() * 2 / 3);
    const int radius = 18;
    for (int y = searchArea.top() + radius; y <= searchArea.bottom() - radius; ++y) {
        for (int x = searchArea.left() + radius; x <= searchArea.right() - radius; ++x) {
            if (!isPdfDarkPixel(page.pixelColor(x, y))) {
                continue;
            }

            int redCount = 0;
            int lightCount = 0;
            int darkCount = 0;
            for (int sampleY = y - radius; sampleY <= y + radius; ++sampleY) {
                for (int sampleX = x - radius; sampleX <= x + radius; ++sampleX) {
                    const QColor sample = page.pixelColor(sampleX, sampleY);
                    if (isPdfRedChartPixel(sample)) {
                        ++redCount;
                    }
                    if (isPdfLightPixel(sample)) {
                        ++lightCount;
                    }
                    if (isPdfDarkPixel(sample)) {
                        ++darkCount;
                    }
                }
            }

            if (redCount >= 120 && lightCount >= 40 && darkCount >= 4) {
                return true;
            }
        }
    }
    return false;
}

int denseRedChartColumnCount(const QImage &page) {
    if (page.isNull()) {
        return 0;
    }

    const QRect searchArea(
        0,
        page.height() / 6,
        page.width(),
        page.height() * 2 / 3);
    const int denseColumnThreshold = std::max(30, searchArea.height() / 14);
    int denseColumns = 0;
    for (int x = searchArea.left(); x <= searchArea.right(); ++x) {
        int redPixels = 0;
        for (int y = searchArea.top(); y <= searchArea.bottom(); ++y) {
            if (isPdfRedChartPixel(page.pixelColor(x, y))) {
                ++redPixels;
            }
        }
        if (redPixels >= denseColumnThreshold) {
            ++denseColumns;
        }
    }
    return denseColumns;
}

int bottomMarginBelowRedChartPixels(const QImage &page) {
    if (page.isNull()) {
        return 0;
    }

    for (int y = page.height() - 1; y >= 0; --y) {
        for (int x = 0; x < page.width(); ++x) {
            if (isPdfRedChartPixel(page.pixelColor(x, y))) {
                return page.height() - 1 - y;
            }
        }
    }
    return page.height();
}
}

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    QImage image(3, 2, QImage::Format_ARGB32);
    image.fill(QColor(255, 0, 0).rgba());
    image.setPixel(1, 0, QColor(0, 255, 0).rgba());
    image.setPixel(2, 0, QColor(0, 0, 255).rgba());
    image.setPixel(0, 1, QColor(0, 0, 255).rgba());
    image.setPixel(1, 1, QColor(255, 0, 0, 128).rgba());
    image.setPixel(2, 1, QColor(0, 255, 0, 0).rgba());

    const ImageAnalysisResult result = ImageAnalysis::analyze(image);
    if (!result.ok) return fail(QStringLiteral("Expected generated image to analyze successfully."));
    if (result.width != 3 || result.height != 2) return fail(QStringLiteral("Unexpected image dimensions."));
    if (result.uniqueColorCount() != 4) return fail(QStringLiteral("Unexpected unique color count."));
    if (result.transparentPixels != 1) return fail(QStringLiteral("Transparent pixel count was wrong."));

    const ColorEntry *red = findColor(result.colors, QColor(255, 0, 0).rgba());
    const ColorEntry *green = findColor(result.colors, QColor(0, 255, 0).rgba());
    const ColorEntry *blue = findColor(result.colors, QColor(0, 0, 255).rgba());
    const ColorEntry *semiTransparentRed = findColor(result.colors, QColor(255, 0, 0, 128).rgba());
    const ColorEntry *transparentGreen = findColor(result.colors, QColor(0, 255, 0, 0).rgba());

    if (!red || red->pixels != 1) return fail(QStringLiteral("Opaque red pixel count was wrong."));
    if (!green || green->pixels != 1) return fail(QStringLiteral("Green pixel count was wrong."));
    if (!blue || blue->pixels != 2) return fail(QStringLiteral("Blue pixel count was wrong."));
    if (!semiTransparentRed || semiTransparentRed->pixels != 1) return fail(QStringLiteral("Semi-transparent red pixel count was wrong."));
    if (transparentGreen) return fail(QStringLiteral("Fully transparent green should not be counted as a stitch color."));

    if (ImageAnalysis::rgbaToHex(QColor(255, 0, 0, 128).rgba()) != QStringLiteral("#FF000080")) {
        return fail(QStringLiteral("RGBA hex formatting was wrong."));
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return fail(QStringLiteral("Could not create temporary test directory."));

    const QString pngPath = QDir(tempDir.path()).filePath(QStringLiteral("sprite.png"));
    if (!image.save(pngPath, "PNG")) return fail(QStringLiteral("Could not write temporary PNG."));

    const ImageAnalysisResult fileResult = ImageAnalysis::analyzeFile(pngPath);
    if (!fileResult.ok) return fail(QStringLiteral("Expected PNG file to analyze successfully."));
    if (fileResult.width != 3 || fileResult.height != 2) return fail(QStringLiteral("PNG file dimensions were wrong."));
    if (fileResult.uniqueColorCount() != 4) return fail(QStringLiteral("PNG file unique color count was wrong."));
    if (fileResult.transparentPixels != 1) return fail(QStringLiteral("PNG file transparent pixel count was wrong."));
    const ColorEntry *fileGreen = findColor(fileResult.colors, QColor(0, 255, 0).rgba());
    if (!fileGreen || fileGreen->pixels != 1) return fail(QStringLiteral("PNG file opaque green pixel count was wrong."));

    QImage greenBackgroundImage(3, 2, QImage::Format_ARGB32);
    greenBackgroundImage.fill(QColor(71, 167, 47).rgba());
    greenBackgroundImage.setPixel(1, 0, QColor(227, 29, 66).rgba());
    greenBackgroundImage.setPixel(2, 1, QColor(0, 0, 255, 0).rgba());

    const ImageAnalysisResult greenBackgroundDisabled = ImageAnalysis::analyze(greenBackgroundImage);
    if (!greenBackgroundDisabled.ok) return fail(QStringLiteral("Expected green background image to analyze successfully."));
    if (greenBackgroundDisabled.transparentPixels != 1) {
        return fail(QStringLiteral("Disabled background filter should only count real alpha transparency."));
    }
    const ColorEntry *disabledGreen = findColor(greenBackgroundDisabled.colors, QColor(71, 167, 47).rgba());
    if (!disabledGreen || disabledGreen->pixels != 4) {
        return fail(QStringLiteral("Disabled background filter should count opaque green as a stitch color."));
    }

    const PatternModel greenPatternDisabled = PatternModel::fromImage(greenBackgroundImage);
    if (!greenPatternDisabled.ok) return fail(QStringLiteral("Expected disabled green pattern to build successfully."));
    if (greenPatternDisabled.opaqueStitchPixels != 5 || greenPatternDisabled.transparentPixels != 1) {
        return fail(QStringLiteral("Disabled green pattern stitch/no-stitch counts were wrong."));
    }
    if (greenPatternDisabled.uniqueSpriteColorCount() != 2) {
        return fail(QStringLiteral("Disabled green pattern should include green and red source colors."));
    }
    if (!greenPatternDisabled.toCsv().contains(QStringLiteral("#47A72FFF")) ||
        !greenPatternDisabled.toCsv().contains(QStringLiteral(",702,Kelly Green,4\n"))) {
        return fail(QStringLiteral("Disabled green pattern CSV should include the green stitch color."));
    }

    TransparencyOptions greenBackgroundOptions;
    greenBackgroundOptions.treatBackgroundColorAsTransparent = true;
    const ImageAnalysisResult greenBackgroundEnabled = ImageAnalysis::analyze(greenBackgroundImage, greenBackgroundOptions);
    if (!greenBackgroundEnabled.ok) return fail(QStringLiteral("Expected enabled green background image to analyze successfully."));
    if (!greenBackgroundEnabled.backgroundColorTransparencyEnabled || !greenBackgroundEnabled.hasBackgroundColor) {
        return fail(QStringLiteral("Enabled background filter should record the selected background color."));
    }
    if (ImageAnalysis::rgbToHex(greenBackgroundEnabled.backgroundColor) != QStringLiteral("#47A72F")) {
        return fail(QStringLiteral("Enabled background filter should use the top-left RGB color."));
    }
    if (greenBackgroundEnabled.transparentPixels != 5 ||
        greenBackgroundEnabled.alphaTransparentPixels != 1 ||
        greenBackgroundEnabled.backgroundTransparentPixels != 4) {
        return fail(QStringLiteral("Enabled background filter transparent pixel counts were wrong."));
    }
    if (findColor(greenBackgroundEnabled.colors, QColor(71, 167, 47).rgba())) {
        return fail(QStringLiteral("Enabled background filter should not count opaque green as a stitch color."));
    }

    const PatternModel greenPatternEnabled = PatternModel::fromImage(greenBackgroundImage, greenBackgroundOptions);
    if (!greenPatternEnabled.ok) return fail(QStringLiteral("Expected enabled green pattern to build successfully."));
    if (greenPatternEnabled.opaqueStitchPixels != 1 || greenPatternEnabled.transparentPixels != 5) {
        return fail(QStringLiteral("Enabled green pattern stitch/no-stitch counts were wrong."));
    }
    if (greenPatternEnabled.alphaTransparentPixels != 1 || greenPatternEnabled.backgroundTransparentPixels != 4) {
        return fail(QStringLiteral("Enabled green pattern transparency source counts were wrong."));
    }
    if (greenPatternEnabled.uniqueSpriteColorCount() != 1 || greenPatternEnabled.matchedColorCount() != 1) {
        return fail(QStringLiteral("Enabled green pattern should only include the red stitch color."));
    }
    if (findColor(ImageAnalysis::analyze(greenBackgroundImage, greenBackgroundOptions).colors, QColor(71, 167, 47).rgba())) {
        return fail(QStringLiteral("Enabled green pattern analysis should exclude green from unique colors."));
    }
    const QString greenEnabledCsv = greenPatternEnabled.toCsv();
    if (greenEnabledCsv.contains(QStringLiteral("#47A72FFF")) ||
        greenEnabledCsv.contains(QStringLiteral("Kelly Green")) ||
        !greenEnabledCsv.contains(QStringLiteral("#E31D42FF,1,666,Bright Red,1\n"))) {
        return fail(QStringLiteral("Enabled green pattern CSV should only include the red stitch color."));
    }
    const QImage greenEnabledChart = greenPatternEnabled.renderChartPreview(10);
    if (greenEnabledChart.pixelColor(2, 2) != QColor(Qt::white)) {
        return fail(QStringLiteral("Enabled green pattern chart should render green background as blank."));
    }
    if (greenEnabledChart.pixelColor(12, 2) == QColor(Qt::white)) {
        return fail(QStringLiteral("Enabled green pattern chart should still render the red stitch."));
    }

    const DmcMatch exactRed = DmcMatcher::nearest(QColor(227, 29, 66).rgba());
    if (!exactRed.ok) return fail(QStringLiteral("Expected exact DMC match to succeed."));
    if (exactRed.color.number != QStringLiteral("666")) return fail(QStringLiteral("Expected bright red to match DMC 666."));
    if (exactRed.color.name != QStringLiteral("Bright Red")) return fail(QStringLiteral("Expected DMC 666 name to be Bright Red."));
    if (exactRed.distanceSquared != 0) return fail(QStringLiteral("Expected exact DMC red distance to be zero."));

    const DmcMatch exactGreen = DmcMatcher::nearest(QColor(71, 167, 47).rgba());
    if (!exactGreen.ok || exactGreen.color.number != QStringLiteral("702") || exactGreen.distanceSquared != 0) {
        return fail(QStringLiteral("Expected exact DMC green to match DMC 702 with zero distance."));
    }

    const DmcMatch nearBlack = DmcMatcher::nearest(QColor(3, 4, 0).rgba());
    if (!nearBlack.ok || nearBlack.color.number != QStringLiteral("310")) {
        return fail(QStringLiteral("Expected near-black color to match DMC 310."));
    }
    if (nearBlack.distanceSquared != 25) return fail(QStringLiteral("Expected near-black distance to be 25."));

    const DmcMatch neutralDark = DmcMatcher::nearest(QColor(49, 50, 49).rgba());
    if (!neutralDark.ok || neutralDark.color.number == QStringLiteral("934")) {
        return fail(QStringLiteral("Neutral dark colors should not drift to greenish DMC 934."));
    }
    if (neutralDark.color.number != QStringLiteral("3799") &&
        neutralDark.color.number != QStringLiteral("844") &&
        neutralDark.color.number != QStringLiteral("413") &&
        neutralDark.color.number != QStringLiteral("310")) {
        return fail(QStringLiteral("Neutral dark colors should prefer black or dark gray DMC colors."));
    }

    const DmcMatch greenBlack = DmcMatcher::nearest(QColor(49, 57, 25).rgba());
    if (!greenBlack.ok || greenBlack.color.number != QStringLiteral("934")) {
        return fail(QStringLiteral("Actually green-black colors should still be allowed to match DMC 934."));
    }

    const DmcMatch exactWhite = DmcMatcher::nearest(QColor(252, 251, 248).rgba());
    if (!exactWhite.ok) return fail(QStringLiteral("Expected exact White DMC match to succeed."));
    if (exactWhite.color.number != QStringLiteral("White")) return fail(QStringLiteral("Expected White DMC code to remain text."));
    if (exactWhite.color.name != QStringLiteral("White")) return fail(QStringLiteral("Expected White DMC name to be White."));
    if (exactWhite.distanceSquared != 0) return fail(QStringLiteral("Expected exact White DMC distance to be zero."));

    const QStringList expectedChartSymbols{
        QStringLiteral("●"),
        QStringLiteral("○"),
        QStringLiteral("■"),
        QStringLiteral("□"),
        QStringLiteral("▲"),
        QStringLiteral("△"),
        QStringLiteral("◆"),
        QStringLiteral("◇"),
        QStringLiteral("✕"),
        QStringLiteral("+"),
        QStringLiteral("/"),
        QStringLiteral("\\"),
        QStringLiteral("="),
        QStringLiteral("#"),
        QStringLiteral("*")
    };
    const QVector<QColor> symbolSetColors{
        QColor(197, 197, 197), // 01
        QColor(171, 171, 171), // 02
        QColor(144, 143, 144), // 03
        QColor(103, 100, 100), // 04
        QColor(187, 175, 165), // 05
        QColor(180, 162, 147), // 06
        QColor(116, 98, 86),   // 07
        QColor(105, 91, 80),   // 08
        QColor(72, 56, 55),    // 09
        QColor(206, 206, 184), // 10
        QColor(224, 220, 132), // 11
        QColor(207, 198, 98),  // 12
        QColor(152, 194, 156), // 13
        QColor(206, 209, 148), // 14
        QColor(205, 214, 132)  // 15
    };
    QImage symbolSetImage(symbolSetColors.size(), 1, QImage::Format_ARGB32);
    for (int x = 0; x < symbolSetColors.size(); ++x) {
        symbolSetImage.setPixel(x, 0, symbolSetColors[x].rgba());
    }
    const PatternModel symbolSetPattern = PatternModel::fromImage(symbolSetImage);
    if (!symbolSetPattern.ok ||
        symbolSetPattern.matchedColors.size() != expectedChartSymbols.size()) {
        return fail(QStringLiteral("Expected symbol set pattern to build with one matched color per test color."));
    }
    if (symbolSetPattern.matchedColors[0].symbol == QStringLiteral("A") ||
        symbolSetPattern.matchedColors[1].symbol == QStringLiteral("B")) {
        return fail(QStringLiteral("Pattern symbols should not use the old letter-only assignment."));
    }
    const QStringList symbolSetCsvLines = symbolSetPattern.toCsv().trimmed().split(QLatin1Char('\n'));
    for (int i = 0; i < expectedChartSymbols.size(); ++i) {
        if (symbolSetPattern.matchedColors[i].symbol != expectedChartSymbols[i] ||
            !symbolSetCsvLines.value(i + 1).startsWith(expectedChartSymbols[i] + QLatin1Char(','))) {
            return fail(QStringLiteral("Pattern symbols should use the cross-stitch symbol set consistently."));
        }
    }

    QImage dmcSortImage(15, 1, QImage::Format_ARGB32);
    int dmcSortX = 0;
    for (int i = 0; i < 5; ++i) dmcSortImage.setPixel(dmcSortX++, 0, QColor(71, 129, 165).rgba());   // 825
    for (int i = 0; i < 4; ++i) dmcSortImage.setPixel(dmcSortX++, 0, QColor(250, 50, 3).rgba());     // 606
    for (int i = 0; i < 3; ++i) dmcSortImage.setPixel(dmcSortX++, 0, QColor(252, 251, 248).rgba());  // White
    for (int i = 0; i < 2; ++i) dmcSortImage.setPixel(dmcSortX++, 0, QColor(37, 59, 115).rgba());    // 336
    dmcSortImage.setPixel(dmcSortX++, 0, QColor(0, 0, 0).rgba());                                    // 310

    const PatternModel dmcSortPattern = PatternModel::fromImage(dmcSortImage);
    if (!dmcSortPattern.ok) return fail(QStringLiteral("Expected DMC sort pattern to build successfully."));
    const QStringList expectedDmcCodes{
        QStringLiteral("310"),
        QStringLiteral("336"),
        QStringLiteral("606"),
        QStringLiteral("825"),
        QStringLiteral("White")
    };
    if (dmcSortPattern.matchedColors.size() != expectedDmcCodes.size()) {
        return fail(QStringLiteral("DMC sort pattern matched color count was wrong."));
    }
    for (int i = 0; i < expectedDmcCodes.size(); ++i) {
        if (dmcSortPattern.matchedColors[i].dmc.number != expectedDmcCodes[i]) {
            return fail(QStringLiteral("DMC matched colors were not sorted in natural code order."));
        }
        if (dmcSortPattern.matchedColors[i].symbol != expectedChartSymbols[i]) {
            return fail(QStringLiteral("DMC sorted symbol assignment was wrong."));
        }
    }
    const QStringList dmcSortCsvLines = dmcSortPattern.toCsv().trimmed().split(QLatin1Char('\n'));
    for (int i = 0; i < expectedDmcCodes.size(); ++i) {
        const QString expectedFragment = QStringLiteral(",%1,").arg(expectedDmcCodes[i]);
        if (!dmcSortCsvLines.value(i + 1).startsWith(expectedChartSymbols[i] + QLatin1Char(',')) ||
            !dmcSortCsvLines.value(i + 1).contains(expectedFragment)) {
            return fail(QStringLiteral("DMC CSV rows were not sorted in natural code order."));
        }
    }
    const PatternSpriteColor *sortBlue825 = findSpriteColor(dmcSortPattern, QColor(71, 129, 165).rgba());
    const PatternSpriteColor *sortWhite = findSpriteColor(dmcSortPattern, QColor(252, 251, 248).rgba());
    if (!sortBlue825 || sortBlue825->matchedColorIndex != 3 ||
        dmcSortPattern.matchedColors[sortBlue825->matchedColorIndex].symbol != expectedChartSymbols[3]) {
        return fail(QStringLiteral("DMC 825 sprite color should map to the sorted symbol."));
    }
    if (!sortWhite || sortWhite->matchedColorIndex != 4 ||
        dmcSortPattern.matchedColors[sortWhite->matchedColorIndex].symbol != expectedChartSymbols[4]) {
        return fail(QStringLiteral("White sprite color should sort after numeric DMC codes and map to the sorted symbol."));
    }
    const int firstGridSpriteIndex = dmcSortPattern.stitchGrid.value(0, -1);
    if (firstGridSpriteIndex < 0 ||
        dmcSortPattern.spriteColors[firstGridSpriteIndex].matchedColorIndex != 3 ||
        dmcSortPattern.stitchPixels[0].matchedColorIndex != 3) {
        return fail(QStringLiteral("Chart stitch indexes should still point at sorted symbols."));
    }

    QImage patternImage(4, 2, QImage::Format_ARGB32);
    patternImage.fill(QColor(0, 255, 0, 0).rgba());
    patternImage.setPixel(0, 0, QColor(227, 29, 66).rgba());
    patternImage.setPixel(1, 0, QColor(227, 29, 66).rgba());
    patternImage.setPixel(2, 0, QColor(228, 29, 66).rgba());
    patternImage.setPixel(3, 0, QColor(71, 167, 47).rgba());

    const PatternModel pattern = PatternModel::fromImage(patternImage);
    if (!pattern.ok) return fail(QStringLiteral("Expected pattern model to build successfully."));
    if (pattern.imageWidth != 4 || pattern.imageHeight != 2) return fail(QStringLiteral("Pattern model dimensions were wrong."));
    if (pattern.opaqueStitchPixels != 4) return fail(QStringLiteral("Pattern model opaque stitch count was wrong."));
    if (pattern.transparentPixels != 4) return fail(QStringLiteral("Pattern model transparent count was wrong."));
    if (pattern.stitchPixels.size() != 4) return fail(QStringLiteral("Pattern model stitch pixel list size was wrong."));
    if (pattern.noStitchPixels.size() != 4) return fail(QStringLiteral("Pattern model no-stitch pixel list size was wrong."));
    if (pattern.stitchPixels[0].x != 0 || pattern.stitchPixels[0].y != 0) {
        return fail(QStringLiteral("First stitch pixel position was wrong."));
    }
    if (pattern.noStitchPixels[0] != QPoint(0, 1)) {
        return fail(QStringLiteral("First no-stitch pixel position was wrong."));
    }
    if (pattern.uniqueSpriteColorCount() != 3) return fail(QStringLiteral("Pattern model unique sprite color count was wrong."));
    if (pattern.matchedColorCount() != 2) return fail(QStringLiteral("Pattern model matched DMC color count was wrong."));

    const PatternMatchedColor &redMatch = pattern.matchedColors[0];
    if (redMatch.symbol != expectedChartSymbols[0]) return fail(QStringLiteral("First matched color should use the first chart symbol."));
    if (redMatch.dmc.number != QStringLiteral("666")) return fail(QStringLiteral("Expected first matched color to be DMC 666."));
    if (redMatch.stitchCount != 3) return fail(QStringLiteral("DMC 666 stitch count should aggregate two sprite reds."));
    if (redMatch.sourceColorCount() != 2) return fail(QStringLiteral("DMC 666 source color count should be 2."));
    if (!redMatch.sourceSpriteColorHexes.contains(QStringLiteral("#E31D42FF")) ||
        !redMatch.sourceSpriteColorHexes.contains(QStringLiteral("#E41D42FF"))) {
        return fail(QStringLiteral("DMC 666 source sprite hex values were wrong."));
    }

    const PatternMatchedColor &greenMatch = pattern.matchedColors[1];
    if (greenMatch.symbol != expectedChartSymbols[1]) return fail(QStringLiteral("Second matched color should use the second chart symbol."));
    if (greenMatch.dmc.number != QStringLiteral("702")) return fail(QStringLiteral("Expected second matched color to be DMC 702."));
    if (greenMatch.stitchCount != 1) return fail(QStringLiteral("DMC 702 stitch count was wrong."));
    if (greenMatch.sourceColorCount() != 1) return fail(QStringLiteral("DMC 702 source color count should be 1."));

    const QString expectedCsv =
        QStringLiteral("symbol,source_sprite_colors,source_color_count,dmc_code,dmc_name,stitch_count\n")
        + expectedChartSymbols[0] + QStringLiteral(",#E31D42FF; #E41D42FF,2,666,Bright Red,3\n")
        + expectedChartSymbols[1] + QStringLiteral(",#47A72FFF,1,702,Kelly Green,1\n");
    if (pattern.toCsv() != expectedCsv) return fail(QStringLiteral("Pattern CSV output was wrong."));
    if (pattern.finishedSizeText(14) != QStringLiteral("14-count Aida: 0.29 x 0.14 in")) {
        return fail(QStringLiteral("14-count finished size text was wrong."));
    }
    if (pattern.finishedSizeText(16) != QStringLiteral("16-count Aida: 0.25 x 0.13 in")) {
        return fail(QStringLiteral("16-count finished size text was wrong."));
    }
    if (pattern.finishedSizeText(18) != QStringLiteral("18-count Aida: 0.22 x 0.11 in")) {
        return fail(QStringLiteral("18-count finished size text was wrong."));
    }

    const QString csvPath = QDir(tempDir.path()).filePath(QStringLiteral("palette.csv"));
    QString csvError;
    if (!pattern.writeCsvFile(csvPath, &csvError)) return fail(QStringLiteral("Pattern CSV write failed: ") + csvError);
    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) return fail(QStringLiteral("Could not reopen written CSV."));
    if (QString::fromUtf8(csvFile.readAll()) != expectedCsv) return fail(QStringLiteral("Written CSV content was wrong."));

    QImage whiteImage(1, 1, QImage::Format_ARGB32);
    whiteImage.fill(QColor(252, 251, 248).rgba());
    const PatternModel whitePattern = PatternModel::fromImage(whiteImage);
    if (!whitePattern.ok) return fail(QStringLiteral("Expected white pattern to build successfully."));
    const QString whiteCsv = whitePattern.toCsv();
    if (!whiteCsv.contains(QStringLiteral(",White,White,1\n"))) {
        return fail(QStringLiteral("White DMC code should remain textual in CSV export."));
    }

    QImage chartSource(11, 3, QImage::Format_ARGB32);
    chartSource.fill(QColor(0, 0, 0, 0).rgba());
    const PatternModel chartModel = PatternModel::fromImage(chartSource);
    if (!chartModel.ok) return fail(QStringLiteral("Expected chart model to build successfully."));
    const QImage blankChart = chartModel.renderChartPreview(10);
    if (blankChart.size() != QSize(110, 30)) return fail(QStringLiteral("Chart preview dimensions were wrong."));
    if (blankChart.pixelColor(15, 5) != QColor(Qt::white)) return fail(QStringLiteral("Blank chart background should be white."));
    if (blankChart.pixelColor(100, 5) != QColor(120, 120, 120)) return fail(QStringLiteral("Every 10th vertical grid line should be darker."));
    if (blankChart.pixelColor(50, 5) != QColor(210, 0, 0)) return fail(QStringLiteral("Vertical center line should be red."));
    if (blankChart.pixelColor(5, 10) != QColor(210, 0, 0)) return fail(QStringLiteral("Horizontal center line should be red."));

    const QColor redFill = QColor(227, 29, 66);
    const QColor symbolOnlyFill = QColor(250, 250, 250);
    const QVector<QColor> gridAndCenterColors{
        QColor(210, 210, 210),
        QColor(120, 120, 120),
        QColor(210, 0, 0)
    };

    const QImage stitchChart = pattern.renderChartPreview(10, true, ChartMode::ColorAndSymbols);
    if (stitchChart.pixelColor(2, 2) == QColor(Qt::white)) return fail(QStringLiteral("Stitch cell should not render as blank white."));
    if (!cellContainsInkOutsideColors(stitchChart, QRect(1, 1, 8, 8), QVector<QColor>{redFill} + gridAndCenterColors)) {
        return fail(QStringLiteral("Color + Symbols chart should draw centered symbol pixels."));
    }
    if (!cellContainsHighContrastSymbolPixel(stitchChart, QRect(1, 1, 8, 8))) {
        return fail(QStringLiteral("Color + Symbols chart should draw readable high-contrast symbol pixels over colored cells."));
    }
    if (lightBackingPixelCount(stitchChart, QRect(1, 1, 8, 8)) > 28) {
        return fail(QStringLiteral("Color + Symbols chart should not draw a filled light backing behind symbols."));
    }

    const QImage symbolsOnlyChart = pattern.renderChartPreview(10, true, ChartMode::SymbolsOnly);
    if (symbolsOnlyChart.pixelColor(1, 1) != symbolOnlyFill) {
        return fail(QStringLiteral("Symbols Only chart should draw light stitch cells."));
    }
    if (symbolsOnlyChart.pixelColor(35, 15) != QColor(Qt::white)) {
        return fail(QStringLiteral("Symbols Only chart should keep no-stitch cells blank."));
    }
    if (!cellContainsInkOutsideColors(symbolsOnlyChart, QRect(1, 1, 8, 8), QVector<QColor>{symbolOnlyFill} + gridAndCenterColors)) {
        return fail(QStringLiteral("Symbols Only chart should draw centered symbol pixels."));
    }

    const QImage colorsOnlyChart = pattern.renderChartPreview(10, true, ChartMode::ColorsOnly);
    if (colorsOnlyChart.pixelColor(2, 2) != redFill) {
        return fail(QStringLiteral("Colors Only chart should draw colored stitch cells."));
    }
    if (!cellIsSolidColor(colorsOnlyChart, QRect(1, 1, 8, 8), redFill)) {
        return fail(QStringLiteral("Colors Only chart should not draw symbol pixels."));
    }
    if (!cellPixelsDiffer(stitchChart, colorsOnlyChart, QRect(1, 1, 8, 8))) {
        return fail(QStringLiteral("Color + Symbols chart should not render the same stitch-cell pixels as Colors Only."));
    }

    const QString chartPngPath = QDir(tempDir.path()).filePath(QStringLiteral("chart.png"));
    QString chartPngError;
    if (!pattern.writeChartPngFile(chartPngPath, 10, &chartPngError)) {
        return fail(QStringLiteral("Pattern chart PNG write failed: ") + chartPngError);
    }

    QFile chartPngFile(chartPngPath);
    if (!chartPngFile.exists() || chartPngFile.size() <= 0) {
        return fail(QStringLiteral("Pattern chart PNG file was not written."));
    }

    QImage exportedChart;
    if (!exportedChart.load(chartPngPath, "PNG")) {
        return fail(QStringLiteral("Pattern chart PNG could not be reloaded."));
    }
    if (exportedChart.size() != QSize(40, 20)) {
        return fail(QStringLiteral("Pattern chart PNG dimensions were wrong."));
    }
    if (exportedChart.pixelColor(2, 2) == QColor(Qt::white)) {
        return fail(QStringLiteral("Pattern chart PNG should contain colored stitch cells."));
    }
    if (exportedChart.pixelColor(35, 15) != QColor(Qt::white)) {
        return fail(QStringLiteral("Pattern chart PNG should contain blank no-stitch cells."));
    }
    if (exportedChart.pixelColor(10, 5) != QColor(210, 210, 210)) {
        return fail(QStringLiteral("Pattern chart PNG should contain light grid lines."));
    }
    if (exportedChart.pixelColor(0, 5) != QColor(120, 120, 120)) {
        return fail(QStringLiteral("Pattern chart PNG should contain darker 10-stitch grid lines."));
    }
    if (exportedChart.pixelColor(20, 5) != QColor(210, 0, 0) ||
        exportedChart.pixelColor(5, 10) != QColor(210, 0, 0)) {
        return fail(QStringLiteral("Pattern chart PNG should contain red center lines."));
    }

    bool foundSymbolPixel = false;
    for (int y = 1; y < 9 && !foundSymbolPixel; ++y) {
        for (int x = 1; x < 9; ++x) {
            const QColor pixel = exportedChart.pixelColor(x, y);
            if (pixel != redFill &&
                pixel != QColor(210, 210, 210) &&
                pixel != QColor(120, 120, 120) &&
                pixel != QColor(210, 0, 0)) {
                foundSymbolPixel = true;
                break;
            }
        }
    }
    if (!foundSymbolPixel) {
        return fail(QStringLiteral("Pattern chart PNG should contain centered symbol pixels."));
    }
    if (!cellContainsHighContrastSymbolPixel(exportedChart, QRect(1, 1, 8, 8))) {
        return fail(QStringLiteral("Pattern chart PNG should contain readable Color + Symbols text over colored cells."));
    }

    const QString symbolsOnlyPngPath = QDir(tempDir.path()).filePath(QStringLiteral("chart-symbols-only.png"));
    if (!pattern.writeChartPngFile(symbolsOnlyPngPath, 10, &chartPngError, ChartMode::SymbolsOnly)) {
        return fail(QStringLiteral("Symbols Only chart PNG write failed: ") + chartPngError);
    }
    QImage exportedSymbolsOnlyChart;
    if (!exportedSymbolsOnlyChart.load(symbolsOnlyPngPath, "PNG")) {
        return fail(QStringLiteral("Symbols Only chart PNG could not be reloaded."));
    }
    if (exportedSymbolsOnlyChart.pixelColor(1, 1) != symbolOnlyFill ||
        !cellContainsInkOutsideColors(exportedSymbolsOnlyChart, QRect(1, 1, 8, 8), QVector<QColor>{symbolOnlyFill} + gridAndCenterColors)) {
        return fail(QStringLiteral("Symbols Only chart PNG should use the selected chart mode."));
    }

    const QString pdfPath = QDir(tempDir.path()).filePath(QStringLiteral("pattern.pdf"));
    QString pdfError;
    if (!pattern.writePdfFile(pdfPath, QStringLiteral("Test Sprite"), 10, &pdfError)) {
        return fail(QStringLiteral("Pattern PDF write failed: ") + pdfError);
    }

    QFile pdfFile(pdfPath);
    if (!pdfFile.exists() || pdfFile.size() <= 0) {
        return fail(QStringLiteral("Pattern PDF file was not written."));
    }
    if (!pdfFile.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Pattern PDF file could not be reopened."));
    }
    if (pdfFile.read(4) != QByteArray("%PDF")) {
        return fail(QStringLiteral("Pattern PDF file did not contain a PDF header."));
    }
    pdfFile.seek(0);
    const QByteArray pdfBytes = pdfFile.readAll();
    if (pdfBytes.contains("PDF layout:") ||
        pdfBytes.contains("Blank cells are unstitched background.") ||
        pdfBytes.contains("Grid labels appear every 10 stitches.") ||
        pdfBytes.contains("v2.9.5")) {
        return fail(QStringLiteral("Pattern PDF should not contain legacy debug/status text."));
    }
    if (pdfBytes.contains("/Subtype /Image") || pdfBytes.contains("/Subtype/Image")) {
        return fail(QStringLiteral("Pattern PDF charts should be drawn directly, not embedded as raster images."));
    }
    const QString pdfToText = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (!pdfToText.isEmpty()) {
        const QString pdfTextPath = QDir(tempDir.path()).filePath(QStringLiteral("pattern.txt"));
        const int exitCode = QProcess::execute(pdfToText, {pdfPath, pdfTextPath});
        if (exitCode != 0) {
            return fail(QStringLiteral("pdftotext could not read the generated PDF."));
        }
        QFile pdfTextFile(pdfTextPath);
        if (!pdfTextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return fail(QStringLiteral("Could not read generated PDF text."));
        }
        const QString pdfText = QString::fromUtf8(pdfTextFile.readAll());
        const qsizetype colorChartIndex = pdfText.indexOf(QStringLiteral("Color Chart"));
        const qsizetype symbolChartIndex = pdfText.indexOf(QStringLiteral("Black-and-White Symbol Chart"));
        const qsizetype firstLegendIndex = pdfText.indexOf(QStringLiteral("Legend"), colorChartIndex);
        const qsizetype secondLegendIndex = pdfText.indexOf(QStringLiteral("Legend"), symbolChartIndex);
        if (colorChartIndex < 0 || symbolChartIndex < 0 || firstLegendIndex < 0 || secondLegendIndex < 0) {
            return fail(QStringLiteral("Pattern PDF should include both chart section titles and the legend title."));
        }
        if (!(colorChartIndex < firstLegendIndex && firstLegendIndex < symbolChartIndex && symbolChartIndex < secondLegendIndex)) {
            return fail(QStringLiteral("Pattern PDF should place a side legend with each chart page."));
        }

        QImage samePageLegendImage(95, 10, QImage::Format_ARGB32);
        const QVector<QColor> samePageLegendColors{
            QColor(227, 29, 66),
            QColor(71, 167, 47),
            QColor(252, 251, 248),
            QColor(0, 0, 0),
            QColor(71, 129, 165)
        };
        for (int y = 0; y < samePageLegendImage.height(); ++y) {
            for (int x = 0; x < samePageLegendImage.width(); ++x) {
                samePageLegendImage.setPixel(x, y, samePageLegendColors[(x / 19) % samePageLegendColors.size()].rgba());
            }
        }

        const PatternModel samePageLegendPattern = PatternModel::fromImage(samePageLegendImage);
        if (!samePageLegendPattern.ok || samePageLegendPattern.matchedColors.size() != samePageLegendColors.size()) {
            return fail(QStringLiteral("Expected same-page legend PDF pattern to build with five matched colors."));
        }

        const QString samePageLegendPdfPath = QDir(tempDir.path()).filePath(QStringLiteral("same-page-legend-pattern.pdf"));
        if (!samePageLegendPattern.writePdfFile(samePageLegendPdfPath, QStringLiteral("Same Page Legend Sprite"), 10, &pdfError)) {
            return fail(QStringLiteral("Same-page legend PDF write failed: ") + pdfError);
        }

        const QString samePageLegendTextPath = QDir(tempDir.path()).filePath(QStringLiteral("same-page-legend-pattern.txt"));
        const int samePageLegendExitCode = QProcess::execute(pdfToText, {samePageLegendPdfPath, samePageLegendTextPath});
        if (samePageLegendExitCode != 0) {
            return fail(QStringLiteral("pdftotext could not read the same-page legend PDF."));
        }
        QFile samePageLegendTextFile(samePageLegendTextPath);
        if (!samePageLegendTextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return fail(QStringLiteral("Could not read generated same-page legend PDF text."));
        }
        const QString samePageLegendPdfText = QString::fromUtf8(samePageLegendTextFile.readAll());
        auto sectionPageHasLegend = [](const QString &pdfText, const QString &sectionTitle) {
            const qsizetype sectionIndex = pdfText.indexOf(sectionTitle);
            if (sectionIndex < 0) {
                return false;
            }
            const qsizetype pageBreakIndex = pdfText.indexOf(QChar::FormFeed, sectionIndex);
            const qsizetype pageStart = std::max<qsizetype>(0, pdfText.lastIndexOf(QChar::FormFeed, sectionIndex) + 1);
            const qsizetype pageEnd = pageBreakIndex < 0 ? pdfText.size() : pageBreakIndex;
            const qsizetype legendIndex = pdfText.indexOf(QStringLiteral("Legend"), pageStart);
            return legendIndex >= 0 && legendIndex < pageEnd;
        };
        if (!sectionPageHasLegend(samePageLegendPdfText, QStringLiteral("Color Chart")) ||
            !sectionPageHasLegend(samePageLegendPdfText, QStringLiteral("Black-and-White Symbol Chart"))) {
            return fail(QStringLiteral("Small wide PDF chart sections should keep a fitting legend on the same page."));
        }

        QImage rightSideLegendImage(30, 70, QImage::Format_ARGB32);
        for (int y = 0; y < rightSideLegendImage.height(); ++y) {
            for (int x = 0; x < rightSideLegendImage.width(); ++x) {
                rightSideLegendImage.setPixel(x, y, samePageLegendColors[(x / 6) % samePageLegendColors.size()].rgba());
            }
        }

        const PatternModel rightSideLegendPattern = PatternModel::fromImage(rightSideLegendImage);
        if (!rightSideLegendPattern.ok || rightSideLegendPattern.matchedColors.size() != samePageLegendColors.size()) {
            return fail(QStringLiteral("Expected right-side legend PDF pattern to build with five matched colors."));
        }

        const QString rightSideLegendPdfPath = QDir(tempDir.path()).filePath(QStringLiteral("right-side-legend-pattern.pdf"));
        if (!rightSideLegendPattern.writePdfFile(rightSideLegendPdfPath, QStringLiteral("Right Side Legend Sprite"), 10, &pdfError)) {
            return fail(QStringLiteral("Right-side legend PDF write failed: ") + pdfError);
        }

        const QString rightSideLegendTextPath = QDir(tempDir.path()).filePath(QStringLiteral("right-side-legend-pattern.txt"));
        const int rightSideLegendExitCode = QProcess::execute(pdfToText, {rightSideLegendPdfPath, rightSideLegendTextPath});
        if (rightSideLegendExitCode != 0) {
            return fail(QStringLiteral("pdftotext could not read the right-side legend PDF."));
        }
        QFile rightSideLegendTextFile(rightSideLegendTextPath);
        if (!rightSideLegendTextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return fail(QStringLiteral("Could not read generated right-side legend PDF text."));
        }
        const QString rightSideLegendPdfText = QString::fromUtf8(rightSideLegendTextFile.readAll());
        if (!sectionPageHasLegend(rightSideLegendPdfText, QStringLiteral("Color Chart")) ||
            !sectionPageHasLegend(rightSideLegendPdfText, QStringLiteral("Black-and-White Symbol Chart"))) {
            return fail(QStringLiteral("Medium PDF chart sections should use a same-page right-side legend when readable."));
        }

        const QString pdfToPpm = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
        if (!pdfToPpm.isEmpty()) {
            QImage pdfSymbolImage(4, 4, QImage::Format_ARGB32);
            pdfSymbolImage.fill(redFill.rgba());
            const PatternModel pdfSymbolPattern = PatternModel::fromImage(pdfSymbolImage);
            if (!pdfSymbolPattern.ok) {
                return fail(QStringLiteral("Expected PDF symbol-overlay pattern to build successfully."));
            }

            const QString pdfSymbolPath = QDir(tempDir.path()).filePath(QStringLiteral("pdf-symbol-overlay.pdf"));
            if (!pdfSymbolPattern.writePdfFile(pdfSymbolPath, QStringLiteral("PDF Symbol Overlay Sprite"), 10, &pdfError)) {
                return fail(QStringLiteral("PDF symbol-overlay write failed: ") + pdfError);
            }

            const QString firstPagePrefix = QDir(tempDir.path()).filePath(QStringLiteral("pdf-symbol-overlay-page1"));
            const int rasterExitCode = QProcess::execute(pdfToPpm, {
                QStringLiteral("-f"),
                QStringLiteral("1"),
                QStringLiteral("-l"),
                QStringLiteral("1"),
                QStringLiteral("-singlefile"),
                QStringLiteral("-png"),
                QStringLiteral("-r"),
                QStringLiteral("144"),
                pdfSymbolPath,
                firstPagePrefix
            });
            if (rasterExitCode != 0) {
                return fail(QStringLiteral("pdftoppm could not rasterize the PDF symbol-overlay page."));
            }

            QImage firstPageImage;
            if (!firstPageImage.load(firstPagePrefix + QStringLiteral(".png"), "PNG")) {
                return fail(QStringLiteral("Could not load rasterized PDF symbol-overlay page."));
            }
            if (!pageContainsColorChartSymbolOverlay(firstPageImage)) {
                return fail(QStringLiteral("Standard PDF color chart should render symbols over colored cells."));
            }

            QImage wideChartImage(80, 20, QImage::Format_ARGB32);
            wideChartImage.fill(redFill.rgba());
            const PatternModel wideChartPattern = PatternModel::fromImage(wideChartImage);
            if (!wideChartPattern.ok) {
                return fail(QStringLiteral("Expected wide PDF layout pattern to build successfully."));
            }

            const QString wideChartPdfPath = QDir(tempDir.path()).filePath(QStringLiteral("wide-chart-layout.pdf"));
            if (!wideChartPattern.writePdfFile(wideChartPdfPath, QStringLiteral("Wide Chart Layout Sprite"), 10, &pdfError)) {
                return fail(QStringLiteral("Wide chart layout PDF write failed: ") + pdfError);
            }

            const QString wideChartPagePrefix = QDir(tempDir.path()).filePath(QStringLiteral("wide-chart-layout-page1"));
            const int wideChartRasterExitCode = QProcess::execute(pdfToPpm, {
                QStringLiteral("-f"),
                QStringLiteral("1"),
                QStringLiteral("-l"),
                QStringLiteral("1"),
                QStringLiteral("-singlefile"),
                QStringLiteral("-png"),
                QStringLiteral("-r"),
                QStringLiteral("144"),
                wideChartPdfPath,
                wideChartPagePrefix
            });
            if (wideChartRasterExitCode != 0) {
                return fail(QStringLiteral("pdftoppm could not rasterize the wide chart layout page."));
            }

            QImage wideChartPageImage;
            if (!wideChartPageImage.load(wideChartPagePrefix + QStringLiteral(".png"), "PNG")) {
                return fail(QStringLiteral("Could not load rasterized wide chart layout page."));
            }
            if (denseRedChartColumnCount(wideChartPageImage) < wideChartPageImage.width() * 65 / 100) {
                return fail(QStringLiteral("Wide PDF chart layout should prioritize chart width when side legend would shrink cells."));
            }

            QImage tallChartImage(20, 80, QImage::Format_ARGB32);
            tallChartImage.fill(redFill.rgba());
            const PatternModel tallChartPattern = PatternModel::fromImage(tallChartImage);
            if (!tallChartPattern.ok) {
                return fail(QStringLiteral("Expected tall PDF margin pattern to build successfully."));
            }

            const QString tallChartPdfPath = QDir(tempDir.path()).filePath(QStringLiteral("tall-chart-margin.pdf"));
            if (!tallChartPattern.writePdfFile(tallChartPdfPath, QStringLiteral("Tall Chart Margin Sprite"), 10, &pdfError)) {
                return fail(QStringLiteral("Tall chart margin PDF write failed: ") + pdfError);
            }

            const QString tallChartPagePrefix = QDir(tempDir.path()).filePath(QStringLiteral("tall-chart-margin-page1"));
            const int tallChartRasterExitCode = QProcess::execute(pdfToPpm, {
                QStringLiteral("-f"),
                QStringLiteral("1"),
                QStringLiteral("-l"),
                QStringLiteral("1"),
                QStringLiteral("-singlefile"),
                QStringLiteral("-png"),
                QStringLiteral("-r"),
                QStringLiteral("144"),
                tallChartPdfPath,
                tallChartPagePrefix
            });
            if (tallChartRasterExitCode != 0) {
                return fail(QStringLiteral("pdftoppm could not rasterize the tall chart margin page."));
            }

            QImage tallChartPageImage;
            if (!tallChartPageImage.load(tallChartPagePrefix + QStringLiteral(".png"), "PNG")) {
                return fail(QStringLiteral("Could not load rasterized tall chart margin page."));
            }
            const int bottomChartMargin = bottomMarginBelowRedChartPixels(tallChartPageImage);
            if (bottomChartMargin < 32) {
                return fail(QStringLiteral("PDF chart pages should keep a clear bottom margin below the chart; got %1 px.")
                    .arg(bottomChartMargin));
            }
        }

        QImage gridNumberImage(21, 21, QImage::Format_ARGB32);
        gridNumberImage.fill(redFill.rgba());
        const PatternModel gridNumberPattern = PatternModel::fromImage(gridNumberImage);
        if (!gridNumberPattern.ok) {
            return fail(QStringLiteral("Expected grid-number PDF pattern to build successfully."));
        }

        const QString gridNumberPdfPath = QDir(tempDir.path()).filePath(QStringLiteral("grid-number-pattern.pdf"));
        if (!gridNumberPattern.writePdfFile(gridNumberPdfPath, QStringLiteral("Grid Number Sprite"), 10, &pdfError)) {
            return fail(QStringLiteral("Grid-number PDF write failed: ") + pdfError);
        }

        const QString gridNumberTextPath = QDir(tempDir.path()).filePath(QStringLiteral("grid-number-pattern.txt"));
        const int gridNumberExitCode = QProcess::execute(pdfToText, {gridNumberPdfPath, gridNumberTextPath});
        if (gridNumberExitCode != 0) {
            return fail(QStringLiteral("pdftotext could not read the grid-number PDF."));
        }
        QFile gridNumberTextFile(gridNumberTextPath);
        if (!gridNumberTextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return fail(QStringLiteral("Could not read generated grid-number PDF text."));
        }
        const QString gridNumberPdfText = QString::fromUtf8(gridNumberTextFile.readAll());
        if (!gridNumberPdfText.contains(QStringLiteral("10")) ||
            !gridNumberPdfText.contains(QStringLiteral("20"))) {
            return fail(QStringLiteral("Pattern PDF charts should include 10-stitch grid number labels."));
        }

        QImage tiledChartImage(201, 201, QImage::Format_ARGB32);
        tiledChartImage.fill(redFill.rgba());
        const PatternModel tiledChartPattern = PatternModel::fromImage(tiledChartImage);
        if (!tiledChartPattern.ok) {
            return fail(QStringLiteral("Expected tiled PDF pattern to build successfully."));
        }

        const QString tiledChartPdfPath = QDir(tempDir.path()).filePath(QStringLiteral("tiled-chart-pattern.pdf"));
        if (!tiledChartPattern.writePdfFile(tiledChartPdfPath, QStringLiteral("Tiled Chart Sprite"), 10, &pdfError)) {
            return fail(QStringLiteral("Tiled chart PDF write failed: ") + pdfError);
        }

        QFile tiledChartPdfFile(tiledChartPdfPath);
        if (!tiledChartPdfFile.open(QIODevice::ReadOnly)) {
            return fail(QStringLiteral("Tiled chart PDF file could not be reopened."));
        }
        const QByteArray tiledChartPdfBytes = tiledChartPdfFile.readAll();
        if (tiledChartPdfBytes.contains("/Subtype /Image") || tiledChartPdfBytes.contains("/Subtype/Image")) {
            return fail(QStringLiteral("Tiled PDF charts should be drawn directly, not embedded as raster images."));
        }

        const QString tiledChartTextPath = QDir(tempDir.path()).filePath(QStringLiteral("tiled-chart-pattern.txt"));
        const int tiledChartExitCode = QProcess::execute(pdfToText, {tiledChartPdfPath, tiledChartTextPath});
        if (tiledChartExitCode != 0) {
            return fail(QStringLiteral("pdftotext could not read the tiled chart PDF."));
        }
        QFile tiledChartTextFile(tiledChartTextPath);
        if (!tiledChartTextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return fail(QStringLiteral("Could not read generated tiled chart PDF text."));
        }
        const QString tiledChartPdfText = QString::fromUtf8(tiledChartTextFile.readAll());
        if (tiledChartPdfText.contains(QStringLiteral("Color Chart - Tile"))) {
            return fail(QStringLiteral("Large PDF export should not include tiled color chart pages."));
        }
        if (!tiledChartPdfText.contains(QStringLiteral("Pattern Info")) ||
            !tiledChartPdfText.contains(QStringLiteral("Legend")) ||
            !tiledChartPdfText.contains(QStringLiteral("Color Overview")) ||
            !tiledChartPdfText.contains(QStringLiteral("Black-and-White Symbol Chart - Tile 4 of 9")) ||
            !tiledChartPdfText.contains(QStringLiteral("Columns 1-100, Rows 101-200")) ||
            !tiledChartPdfText.contains(QStringLiteral("Columns 201-201, Rows 201-201"))) {
            return fail(QStringLiteral("Large PDF charts should include tile titles with absolute stitch ranges."));
        }
        if (!tiledChartPdfText.contains(QStringLiteral("110"))) {
            return fail(QStringLiteral("Tiled PDF grid numbers should use absolute stitch coordinates."));
        }

        PdfExportOptions tiledColorOptions;
        tiledColorOptions.includePatternInfo = false;
        tiledColorOptions.includeLegend = false;
        tiledColorOptions.includeColorOverview = false;
        tiledColorOptions.includeBlackAndWhiteSymbolChart = false;
        tiledColorOptions.includeTiledColorChart = true;
        tiledColorOptions.tileSize = 75;

        const QString tiledColorPdfPath = QDir(tempDir.path()).filePath(QStringLiteral("tiled-color-chart-pattern.pdf"));
        if (!tiledChartPattern.writePdfFile(tiledColorPdfPath, QStringLiteral("Tiled Color Chart Sprite"), 10, &pdfError, tiledColorOptions)) {
            return fail(QStringLiteral("Tiled color chart PDF write failed: ") + pdfError);
        }

        const QString tiledColorTextPath = QDir(tempDir.path()).filePath(QStringLiteral("tiled-color-chart-pattern.txt"));
        const int tiledColorExitCode = QProcess::execute(pdfToText, {tiledColorPdfPath, tiledColorTextPath});
        if (tiledColorExitCode != 0) {
            return fail(QStringLiteral("pdftotext could not read the tiled color chart PDF."));
        }
        QFile tiledColorTextFile(tiledColorTextPath);
        if (!tiledColorTextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return fail(QStringLiteral("Could not read generated tiled color chart PDF text."));
        }
        const QString tiledColorPdfText = QString::fromUtf8(tiledColorTextFile.readAll());
        if (!tiledColorPdfText.contains(QStringLiteral("Color Chart - Tile 4 of 9")) ||
            !tiledColorPdfText.contains(QStringLiteral("Columns 1-75, Rows 76-150")) ||
            tiledColorPdfText.contains(QStringLiteral("Black-and-White Symbol Chart"))) {
            return fail(QStringLiteral("PDF export options should allow tiled color chart pages with the selected tile size."));
        }
    }

    const PatternModel emptyPattern = PatternModel::fromImage(QImage());
    if (emptyPattern.ok) return fail(QStringLiteral("Null image should not build a pattern model."));

    const ImageAnalysisResult empty = ImageAnalysis::analyze(QImage());
    if (empty.ok) return fail(QStringLiteral("Null image should not analyze successfully."));

    return 0;
}
