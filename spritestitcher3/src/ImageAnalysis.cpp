#include "ImageAnalysis.h"

#include <QHash>
#include <QImageReader>

#include <algorithm>

int ImageAnalysisResult::uniqueColorCount() const {
    return colors.size();
}

namespace ImageAnalysis {

namespace {
QRgb rgbOnly(QRgb rgba) {
    return qRgb(qRed(rgba), qGreen(rgba), qBlue(rgba));
}

bool rgbMatches(QRgb a, QRgb b) {
    return qRed(a) == qRed(b) &&
           qGreen(a) == qGreen(b) &&
           qBlue(a) == qBlue(b);
}
}

QString rgbaToHex(QRgb rgba) {
    return QStringLiteral("#%1%2%3%4")
        .arg(qRed(rgba), 2, 16, QChar('0'))
        .arg(qGreen(rgba), 2, 16, QChar('0'))
        .arg(qBlue(rgba), 2, 16, QChar('0'))
        .arg(qAlpha(rgba), 2, 16, QChar('0'))
        .toUpper();
}

QString rgbToHex(QRgb rgba) {
    return QStringLiteral("#%1%2%3")
        .arg(qRed(rgba), 2, 16, QChar('0'))
        .arg(qGreen(rgba), 2, 16, QChar('0'))
        .arg(qBlue(rgba), 2, 16, QChar('0'))
        .toUpper();
}

TransparencyOptions resolveTransparencyOptions(const QImage &source, const TransparencyOptions &options) {
    TransparencyOptions resolved = options;
    if (resolved.hasBackgroundColor) {
        resolved.backgroundColor = rgbOnly(resolved.backgroundColor);
        return resolved;
    }

    if (!resolved.treatBackgroundColorAsTransparent || source.isNull()) {
        return resolved;
    }

    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    resolved.backgroundColor = rgbOnly(image.pixel(0, 0));
    resolved.hasBackgroundColor = true;
    return resolved;
}

bool isNoStitchPixel(QRgb rgba, const TransparencyOptions &options) {
    if (qAlpha(rgba) == 0) {
        return true;
    }

    return options.treatBackgroundColorAsTransparent &&
           options.hasBackgroundColor &&
           rgbMatches(rgba, options.backgroundColor);
}

QImage filteredImage(const QImage &source, const TransparencyOptions &options) {
    if (source.isNull()) {
        return QImage();
    }

    QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const TransparencyOptions resolved = resolveTransparencyOptions(image, options);
    if (!resolved.treatBackgroundColorAsTransparent || !resolved.hasBackgroundColor) {
        return image;
    }

    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) == 0) {
                continue;
            }
            if (rgbMatches(line[x], resolved.backgroundColor)) {
                line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]), 0);
            }
        }
    }

    return image;
}

ImageAnalysisResult analyze(const QImage &source, const TransparencyOptions &options) {
    ImageAnalysisResult result;
    if (source.isNull()) {
        result.error = QStringLiteral("The selected image could not be loaded.");
        return result;
    }

    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    result.ok = true;
    result.width = image.width();
    result.height = image.height();

    const TransparencyOptions resolved = resolveTransparencyOptions(image, options);
    result.backgroundColorTransparencyEnabled = resolved.treatBackgroundColorAsTransparent;
    result.hasBackgroundColor = resolved.hasBackgroundColor;
    result.backgroundColor = resolved.backgroundColor;

    QHash<QRgb, int> counts;
    counts.reserve(image.width() * image.height());

    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) == 0) {
                ++result.transparentPixels;
                ++result.alphaTransparentPixels;
                continue;
            }
            if (resolved.treatBackgroundColorAsTransparent &&
                resolved.hasBackgroundColor &&
                rgbMatches(line[x], resolved.backgroundColor)) {
                ++result.transparentPixels;
                ++result.backgroundTransparentPixels;
                continue;
            }
            ++counts[line[x]];
        }
    }

    result.colors.reserve(counts.size());
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        result.colors.push_back(ColorEntry{it.key(), it.value()});
    }

    std::sort(result.colors.begin(), result.colors.end(), [](const ColorEntry &a, const ColorEntry &b) {
        if (a.pixels != b.pixels) return a.pixels > b.pixels;
        return rgbaToHex(a.rgba) < rgbaToHex(b.rgba);
    });

    return result;
}

ImageAnalysisResult analyzeFile(const QString &path, const TransparencyOptions &options) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();

    ImageAnalysisResult result = analyze(image, options);
    if (!result.ok && reader.error() != QImageReader::UnknownError) {
        result.error = reader.errorString();
    }
    return result;
}

}
