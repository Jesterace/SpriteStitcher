#include "ImageAnalysis.h"

#include "DmcMatcher.h"
#include "PatternModel.h"

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
}

int main() {
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

    const PatternModel emptyPattern = PatternModel::fromImage(QImage());
    if (emptyPattern.ok) return fail(QStringLiteral("Null image should not build a pattern model."));

    const ImageAnalysisResult empty = ImageAnalysis::analyze(QImage());
    if (empty.ok) return fail(QStringLiteral("Null image should not analyze successfully."));

    return 0;
}
