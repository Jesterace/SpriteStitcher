#include "ImageAnalysis.h"

#include <QHash>
#include <QImageReader>

#include <algorithm>

int ImageAnalysisResult::uniqueColorCount() const {
    return colors.size();
}

namespace ImageAnalysis {

QString rgbaToHex(QRgb rgba) {
    return QStringLiteral("#%1%2%3%4")
        .arg(qRed(rgba), 2, 16, QChar('0'))
        .arg(qGreen(rgba), 2, 16, QChar('0'))
        .arg(qBlue(rgba), 2, 16, QChar('0'))
        .arg(qAlpha(rgba), 2, 16, QChar('0'))
        .toUpper();
}

ImageAnalysisResult analyze(const QImage &source) {
    ImageAnalysisResult result;
    if (source.isNull()) {
        result.error = QStringLiteral("The selected image could not be loaded.");
        return result;
    }

    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    result.ok = true;
    result.width = image.width();
    result.height = image.height();

    QHash<QRgb, int> counts;
    counts.reserve(image.width() * image.height());

    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) == 0) {
                ++result.transparentPixels;
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

ImageAnalysisResult analyzeFile(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();

    ImageAnalysisResult result = analyze(image);
    if (!result.ok && reader.error() != QImageReader::UnknownError) {
        result.error = reader.errorString();
    }
    return result;
}

}
