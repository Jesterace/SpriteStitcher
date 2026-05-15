#include "ImageAnalysis.h"

#include <QColor>
#include <QDebug>
#include <QDir>
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

    const ImageAnalysisResult empty = ImageAnalysis::analyze(QImage());
    if (empty.ok) return fail(QStringLiteral("Null image should not analyze successfully."));

    return 0;
}
