#include "PatternModel.h"

#include <QFile>
#include <QHash>
#include <QFont>
#include <QImageWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QPdfWriter>
#include <QPoint>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace {
int pointsToPixels(double points, int dpi) {
    return static_cast<int>(std::round(points * dpi / 72.0));
}

QString csvEscape(const QString &value) {
    QString escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    if (escaped.contains(QLatin1Char(',')) ||
        escaped.contains(QLatin1Char('"')) ||
        escaped.contains(QLatin1Char('\n')) ||
        escaped.contains(QLatin1Char('\r'))) {
        return QStringLiteral("\"") + escaped + QStringLiteral("\"");
    }
    return escaped;
}

void drawElidedText(QPainter &painter, const QRect &rect, const QString &text, int flags) {
    const int padding = 6;
    QRect padded = rect.adjusted(padding, 0, -padding, 0);
    const QString elided = painter.fontMetrics().elidedText(text, Qt::ElideRight, padded.width());
    painter.drawText(padded, flags, elided);
}
}

PatternModel PatternModel::fromImage(const QImage &image, const TransparencyOptions &options) {
    const TransparencyOptions resolvedOptions = ImageAnalysis::resolveTransparencyOptions(image, options);
    PatternModel model = fromAnalysis(ImageAnalysis::analyze(image, resolvedOptions));
    if (!model.ok) {
        return model;
    }

    QHash<QRgb, int> spriteIndexByRgba;
    for (int i = 0; i < model.spriteColors.size(); ++i) {
        spriteIndexByRgba.insert(model.spriteColors[i].rgba, i);
    }

    const QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);
    model.stitchGrid.fill(-1, model.imageWidth * model.imageHeight);
    model.stitchPixels.reserve(model.opaqueStitchPixels);
    model.noStitchPixels.reserve(model.transparentPixels);

    for (int y = 0; y < argbImage.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(argbImage.constScanLine(y));
        for (int x = 0; x < argbImage.width(); ++x) {
            const QRgb rgba = line[x];
            if (ImageAnalysis::isNoStitchPixel(rgba, resolvedOptions)) {
                model.noStitchPixels.push_back(QPoint(x, y));
                continue;
            }

            const int spriteColorIndex = spriteIndexByRgba.value(rgba, -1);
            const int matchedColorIndex = spriteColorIndex >= 0
                ? model.spriteColors[spriteColorIndex].matchedColorIndex
                : -1;
            if (spriteColorIndex >= 0 && y * model.imageWidth + x < model.stitchGrid.size()) {
                model.stitchGrid[y * model.imageWidth + x] = spriteColorIndex;
            }
            model.stitchPixels.push_back(PatternPixel{x, y, spriteColorIndex, matchedColorIndex});
        }
    }

    model.opaqueStitchPixels = model.stitchPixels.size();
    model.transparentPixels = model.noStitchPixels.size();
    return model;
}

PatternModel PatternModel::fromAnalysis(const ImageAnalysisResult &analysis) {
    PatternModel model;
    model.ok = analysis.ok;
    model.error = analysis.error;
    model.imageWidth = analysis.width;
    model.imageHeight = analysis.height;
    model.transparentPixels = analysis.transparentPixels;
    model.alphaTransparentPixels = analysis.alphaTransparentPixels;
    model.backgroundTransparentPixels = analysis.backgroundTransparentPixels;
    model.backgroundColorTransparencyEnabled = analysis.backgroundColorTransparencyEnabled;
    model.hasBackgroundColor = analysis.hasBackgroundColor;
    model.backgroundColor = analysis.backgroundColor;

    if (!analysis.ok) {
        return model;
    }

    QHash<QString, int> matchedIndexByDmcNumber;
    for (const ColorEntry &entry : analysis.colors) {
        PatternSpriteColor spriteColor;
        spriteColor.rgba = entry.rgba;
        spriteColor.hex = ImageAnalysis::rgbaToHex(entry.rgba);
        spriteColor.pixels = entry.pixels;
        spriteColor.dmcMatch = DmcMatcher::nearest(entry.rgba);
        model.opaqueStitchPixels += entry.pixels;

        const QString dmcNumber = spriteColor.dmcMatch.ok ? spriteColor.dmcMatch.color.number : QString();
        int matchedIndex = matchedIndexByDmcNumber.value(dmcNumber, -1);
        if (matchedIndex < 0) {
            PatternMatchedColor matched;
            matched.symbol = symbolForIndex(model.matchedColors.size());
            if (spriteColor.dmcMatch.ok) {
                matched.dmc = spriteColor.dmcMatch.color;
            }
            matchedIndex = model.matchedColors.size();
            model.matchedColors.push_back(matched);
            matchedIndexByDmcNumber.insert(dmcNumber, matchedIndex);
        }

        spriteColor.matchedColorIndex = matchedIndex;
        const int spriteIndex = model.spriteColors.size();
        model.spriteColors.push_back(spriteColor);

        PatternMatchedColor &matched = model.matchedColors[matchedIndex];
        matched.stitchCount += entry.pixels;
        matched.spriteColorIndexes.push_back(spriteIndex);
        matched.sourceSpriteColorHexes.push_back(spriteColor.hex);
    }

    return model;
}

int PatternMatchedColor::sourceColorCount() const {
    return sourceSpriteColorHexes.size();
}

int PatternModel::uniqueSpriteColorCount() const {
    return spriteColors.size();
}

int PatternModel::matchedColorCount() const {
    return matchedColors.size();
}

QImage PatternModel::renderChartPreview(int cellSize, bool drawCenterLines) const {
    if (!ok || imageWidth <= 0 || imageHeight <= 0 || cellSize <= 0) {
        return QImage();
    }

    const QSize outputSize(imageWidth * cellSize, imageHeight * cellSize);
    QImage canvas(outputSize, QImage::Format_ARGB32);
    canvas.fill(Qt::white);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(std::max(6, static_cast<int>(cellSize * 0.55)));
    painter.setFont(font);

    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            const int index = y * imageWidth + x;
            const int spriteIndex = index < stitchGrid.size() ? stitchGrid[index] : -1;
            const QRect rect(x * cellSize, y * cellSize, cellSize, cellSize);
            if (spriteIndex < 0 || spriteIndex >= spriteColors.size()) {
                painter.fillRect(rect, Qt::white);
                continue;
            }

            const PatternSpriteColor &sprite = spriteColors[spriteIndex];
            const int matchedIndex = sprite.matchedColorIndex;
            const QColor fill = (matchedIndex >= 0 && matchedIndex < matchedColors.size())
                ? matchedColors[matchedIndex].dmc.color
                : QColor::fromRgba(sprite.rgba);
            painter.fillRect(rect, fill);

            const int luminance = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
            painter.setPen(luminance < 128 ? Qt::white : Qt::black);
            const QString symbol = (matchedIndex >= 0 && matchedIndex < matchedColors.size())
                ? matchedColors[matchedIndex].symbol
                : QString();
            painter.drawText(rect, Qt::AlignCenter, symbol);
        }
    }

    for (int x = 0; x <= imageWidth; ++x) {
        QPen pen((x % 10 == 0) ? QColor(120, 120, 120) : QColor(210, 210, 210));
        pen.setWidth((x % 10 == 0) ? 1 : 1);
        painter.setPen(pen);
        const int px = x * cellSize;
        painter.drawLine(px, 0, px, outputSize.height());
    }
    for (int y = 0; y <= imageHeight; ++y) {
        QPen pen((y % 10 == 0) ? QColor(120, 120, 120) : QColor(210, 210, 210));
        pen.setWidth((y % 10 == 0) ? 1 : 1);
        painter.setPen(pen);
        const int py = y * cellSize;
        painter.drawLine(0, py, outputSize.width(), py);
    }

    if (drawCenterLines) {
        QPen centerPen(QColor(210, 0, 0));
        centerPen.setWidth(2);
        painter.setPen(centerPen);
        const int centerX = (imageWidth / 2) * cellSize;
        const int centerY = (imageHeight / 2) * cellSize;
        painter.drawLine(centerX, 0, centerX, outputSize.height());
        painter.drawLine(0, centerY, outputSize.width(), centerY);
    }

    painter.end();
    return canvas;
}

bool PatternModel::writeChartPngFile(const QString &path, int cellSize, QString *errorMessage) const {
    const QImage chart = renderChartPreview(cellSize, true);
    if (chart.isNull()) {
        if (errorMessage) {
            *errorMessage = ok
                ? QStringLiteral("Could not render chart PNG.")
                : error;
        }
        return false;
    }

    QImageWriter writer(path, "PNG");
    if (!writer.write(chart)) {
        if (errorMessage) {
            *errorMessage = writer.errorString();
        }
        return false;
    }

    return true;
}

bool PatternModel::writePdfFile(const QString &path, const QString &imageName, int chartCellSize, QString *errorMessage) const {
    const QImage chart = renderChartPreview(chartCellSize, true);
    if (chart.isNull()) {
        if (errorMessage) {
            *errorMessage = ok
                ? QStringLiteral("Could not render chart for PDF.")
                : error;
        }
        return false;
    }

    const int dpi = 300;
    QPdfWriter writer(path);
    writer.setResolution(dpi);
    writer.setPageSize(QPageSize(QPageSize::Letter));
    writer.setPageMargins(QMarginsF(36, 36, 36, 36), QPageLayout::Point);
    writer.setCreator(QStringLiteral("SpriteStitcher 3"));
    writer.setTitle(imageName);

    QPainter painter;
    if (!painter.begin(&writer)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create PDF file.");
        }
        return false;
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const QRect content = writer.pageLayout().paintRectPixels(dpi);
    const int gap = pointsToPixels(10, dpi);
    int y = content.top();

    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(16);
    painter.setFont(titleFont);
    painter.setPen(Qt::black);
    const QString title = imageName.trimmed().isEmpty()
        ? QStringLiteral("Untitled Sprite")
        : imageName.trimmed();
    const int titleHeight = painter.fontMetrics().height();
    painter.drawText(QRect(content.left(), y, content.width(), titleHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     title);
    y += titleHeight + pointsToPixels(4, dpi);

    QFont bodyFont = painter.font();
    bodyFont.setBold(false);
    bodyFont.setPointSize(10);
    painter.setFont(bodyFont);
    const int bodyHeight = painter.fontMetrics().height();
    painter.drawText(QRect(content.left(), y, content.width(), bodyHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Pattern size: %1 x %2 stitches").arg(imageWidth).arg(imageHeight));
    y += bodyHeight;
    painter.drawText(QRect(content.left(), y, content.width(), bodyHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Total stitch count: %1").arg(opaqueStitchPixels));
    y += bodyHeight + gap;

    QSize chartTargetSize = chart.size();
    chartTargetSize.scale(content.width(), pointsToPixels(360, dpi), Qt::KeepAspectRatio);
    const QRect chartRect(content.left(), y, chartTargetSize.width(), chartTargetSize.height());
    painter.drawImage(chartRect, chart);
    y = chartRect.bottom() + gap;

    QFont sectionFont = painter.font();
    sectionFont.setBold(true);
    sectionFont.setPointSize(12);
    painter.setFont(sectionFont);
    const int sectionHeight = painter.fontMetrics().height();
    painter.drawText(QRect(content.left(), y, content.width(), sectionHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Legend"));
    y += sectionHeight + pointsToPixels(4, dpi);

    bodyFont.setPointSize(9);
    painter.setFont(bodyFont);
    const int rowHeight = pointsToPixels(20, dpi);
    const int tableWidth = content.width();
    const int symbolWidth = tableWidth * 12 / 100;
    const int codeWidth = tableWidth * 18 / 100;
    const int nameWidth = tableWidth * 38 / 100;
    const int countWidth = tableWidth * 18 / 100;
    const int swatchWidth = tableWidth - symbolWidth - codeWidth - nameWidth - countWidth;
    QVector<int> columnWidths{symbolWidth, codeWidth, nameWidth, countWidth, swatchWidth};
    QStringList headers{
        QStringLiteral("Symbol"),
        QStringLiteral("DMC Code"),
        QStringLiteral("DMC Name"),
        QStringLiteral("Stitch Count"),
        QStringLiteral("Swatch")
    };

    auto drawRow = [&](const QStringList &values, const QColor &swatchColor, bool header) {
        int x = content.left();
        if (header) {
            painter.fillRect(QRect(x, y, tableWidth, rowHeight), QColor(235, 235, 235));
        }
        painter.setPen(QColor(170, 170, 170));
        painter.drawRect(QRect(x, y, tableWidth, rowHeight));
        for (int i = 0; i < columnWidths.size(); ++i) {
            const QRect cell(x, y, columnWidths[i], rowHeight);
            painter.setPen(QColor(170, 170, 170));
            painter.drawRect(cell);
            painter.setPen(Qt::black);
            if (!header && i == 4) {
                const int swatchSize = rowHeight - pointsToPixels(6, dpi);
                const QRect swatch(cell.left() + pointsToPixels(6, dpi),
                                   cell.top() + pointsToPixels(3, dpi),
                                   swatchSize,
                                   swatchSize);
                painter.fillRect(swatch, swatchColor);
                painter.setPen(Qt::black);
                painter.drawRect(swatch);
            } else {
                drawElidedText(painter, cell, values.value(i), Qt::AlignLeft | Qt::AlignVCenter);
            }
            x += columnWidths[i];
        }
        y += rowHeight;
    };

    drawRow(headers, QColor(), true);
    for (const PatternMatchedColor &matched : matchedColors) {
        if (y + rowHeight > content.bottom()) {
            break;
        }
        drawRow({
            matched.symbol,
            matched.dmc.number,
            matched.dmc.name,
            QString::number(matched.stitchCount),
            QString()
        }, matched.dmc.color, false);
    }

    painter.end();
    return true;
}

QString PatternModel::toCsv() const {
    QString csv;
    QTextStream out(&csv);
    out << "symbol,source_sprite_colors,source_color_count,dmc_code,dmc_name,stitch_count\n";
    for (const PatternMatchedColor &matched : matchedColors) {
        out << csvEscape(matched.symbol) << ','
            << csvEscape(matched.sourceSpriteColorHexes.join(QStringLiteral("; "))) << ','
            << matched.sourceColorCount() << ','
            << csvEscape(matched.dmc.number) << ','
            << csvEscape(matched.dmc.name) << ','
            << matched.stitchCount << '\n';
    }
    return csv;
}

bool PatternModel::writeCsvFile(const QString &path, QString *errorMessage) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream out(&file);
    out << toCsv();
    if (out.status() != QTextStream::Ok) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write CSV data.");
        }
        return false;
    }
    return true;
}

QString PatternModel::symbolForIndex(int index) {
    if (index < 0) return QString();

    QString symbol;
    int value = index;
    do {
        const int letter = value % 26;
        symbol.prepend(QChar(QLatin1Char('A' + letter)));
        value = value / 26 - 1;
    } while (value >= 0);

    return symbol;
}
