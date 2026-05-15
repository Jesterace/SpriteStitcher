#include "ImageAnalysis.h"

#include "DmcMatcher.h"
#include "PatternModel.h"

#include <QGuiApplication>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

namespace {
int fail(const QString &message) {
    qWarning().noquote() << message;
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

    const DmcMatch exactWhite = DmcMatcher::nearest(QColor(252, 251, 248).rgba());
    if (!exactWhite.ok) return fail(QStringLiteral("Expected exact White DMC match to succeed."));
    if (exactWhite.color.number != QStringLiteral("White")) return fail(QStringLiteral("Expected White DMC code to remain text."));
    if (exactWhite.color.name != QStringLiteral("White")) return fail(QStringLiteral("Expected White DMC name to be White."));
    if (exactWhite.distanceSquared != 0) return fail(QStringLiteral("Expected exact White DMC distance to be zero."));

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
        if (dmcSortPattern.matchedColors[i].symbol != QString(QChar(QLatin1Char('A' + i)))) {
            return fail(QStringLiteral("DMC sorted symbol assignment was wrong."));
        }
    }
    const QStringList dmcSortCsvLines = dmcSortPattern.toCsv().trimmed().split(QLatin1Char('\n'));
    for (int i = 0; i < expectedDmcCodes.size(); ++i) {
        const QString expectedFragment = QStringLiteral(",%1,").arg(expectedDmcCodes[i]);
        if (!dmcSortCsvLines.value(i + 1).contains(expectedFragment)) {
            return fail(QStringLiteral("DMC CSV rows were not sorted in natural code order."));
        }
    }
    const PatternSpriteColor *sortBlue825 = findSpriteColor(dmcSortPattern, QColor(71, 129, 165).rgba());
    const PatternSpriteColor *sortWhite = findSpriteColor(dmcSortPattern, QColor(252, 251, 248).rgba());
    if (!sortBlue825 || sortBlue825->matchedColorIndex != 3 ||
        dmcSortPattern.matchedColors[sortBlue825->matchedColorIndex].symbol != QStringLiteral("D")) {
        return fail(QStringLiteral("DMC 825 sprite color should map to sorted symbol D."));
    }
    if (!sortWhite || sortWhite->matchedColorIndex != 4 ||
        dmcSortPattern.matchedColors[sortWhite->matchedColorIndex].symbol != QStringLiteral("E")) {
        return fail(QStringLiteral("White sprite color should sort after numeric DMC codes and map to symbol E."));
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
    if (redMatch.symbol != QStringLiteral("A")) return fail(QStringLiteral("First matched color should use symbol A."));
    if (redMatch.dmc.number != QStringLiteral("666")) return fail(QStringLiteral("Expected first matched color to be DMC 666."));
    if (redMatch.stitchCount != 3) return fail(QStringLiteral("DMC 666 stitch count should aggregate two sprite reds."));
    if (redMatch.sourceColorCount() != 2) return fail(QStringLiteral("DMC 666 source color count should be 2."));
    if (!redMatch.sourceSpriteColorHexes.contains(QStringLiteral("#E31D42FF")) ||
        !redMatch.sourceSpriteColorHexes.contains(QStringLiteral("#E41D42FF"))) {
        return fail(QStringLiteral("DMC 666 source sprite hex values were wrong."));
    }

    const PatternMatchedColor &greenMatch = pattern.matchedColors[1];
    if (greenMatch.symbol != QStringLiteral("B")) return fail(QStringLiteral("Second matched color should use symbol B."));
    if (greenMatch.dmc.number != QStringLiteral("702")) return fail(QStringLiteral("Expected second matched color to be DMC 702."));
    if (greenMatch.stitchCount != 1) return fail(QStringLiteral("DMC 702 stitch count was wrong."));
    if (greenMatch.sourceColorCount() != 1) return fail(QStringLiteral("DMC 702 source color count should be 1."));

    const QString expectedCsv =
        QStringLiteral("symbol,source_sprite_colors,source_color_count,dmc_code,dmc_name,stitch_count\n")
        + QStringLiteral("A,#E31D42FF; #E41D42FF,2,666,Bright Red,3\n")
        + QStringLiteral("B,#47A72FFF,1,702,Kelly Green,1\n");
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
    if (!pattern.writePdfFile(pdfPath, QStringLiteral("Test Sprite"), 10, &pdfError, ChartMode::ColorsOnly)) {
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

    const PatternModel emptyPattern = PatternModel::fromImage(QImage());
    if (emptyPattern.ok) return fail(QStringLiteral("Null image should not build a pattern model."));

    const ImageAnalysisResult empty = ImageAnalysis::analyze(QImage());
    if (empty.ok) return fail(QStringLiteral("Null image should not analyze successfully."));

    return 0;
}
