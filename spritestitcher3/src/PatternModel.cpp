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

QString formatInches(double inches) {
    QString text = QString::number(inches, 'f', 2);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
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

bool dmcCodeLess(const QString &left, const QString &right) {
    bool leftNumeric = false;
    bool rightNumeric = false;
    const int leftNumber = left.toInt(&leftNumeric);
    const int rightNumber = right.toInt(&rightNumeric);

    if (leftNumeric && rightNumeric && leftNumber != rightNumber) {
        return leftNumber < rightNumber;
    }
    if (leftNumeric != rightNumeric) {
        return leftNumeric;
    }

    const int insensitive = QString::compare(left, right, Qt::CaseInsensitive);
    if (insensitive != 0) {
        return insensitive < 0;
    }
    return left < right;
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

    QVector<int> sortedMatchedIndexes;
    sortedMatchedIndexes.reserve(model.matchedColors.size());
    for (int i = 0; i < model.matchedColors.size(); ++i) {
        sortedMatchedIndexes.push_back(i);
    }

    std::stable_sort(sortedMatchedIndexes.begin(), sortedMatchedIndexes.end(), [&](int left, int right) {
        const PatternMatchedColor &leftMatched = model.matchedColors[left];
        const PatternMatchedColor &rightMatched = model.matchedColors[right];
        if (leftMatched.dmc.number != rightMatched.dmc.number) {
            return dmcCodeLess(leftMatched.dmc.number, rightMatched.dmc.number);
        }
        const int nameCompare = QString::compare(leftMatched.dmc.name, rightMatched.dmc.name, Qt::CaseInsensitive);
        if (nameCompare != 0) {
            return nameCompare < 0;
        }
        return left < right;
    });

    QVector<int> oldMatchedIndexToNew(model.matchedColors.size(), -1);
    QVector<PatternMatchedColor> sortedMatchedColors;
    sortedMatchedColors.reserve(model.matchedColors.size());
    for (int newIndex = 0; newIndex < sortedMatchedIndexes.size(); ++newIndex) {
        const int oldIndex = sortedMatchedIndexes[newIndex];
        PatternMatchedColor matched = model.matchedColors[oldIndex];
        matched.symbol = symbolForIndex(newIndex);
        oldMatchedIndexToNew[oldIndex] = newIndex;
        sortedMatchedColors.push_back(matched);
    }
    model.matchedColors = sortedMatchedColors;

    for (PatternSpriteColor &spriteColor : model.spriteColors) {
        const int oldIndex = spriteColor.matchedColorIndex;
        if (oldIndex >= 0 && oldIndex < oldMatchedIndexToNew.size()) {
            spriteColor.matchedColorIndex = oldMatchedIndexToNew[oldIndex];
        }
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

QString PatternModel::finishedSizeText(int fabricCount) const {
    if (fabricCount <= 0 || imageWidth <= 0 || imageHeight <= 0) {
        return QString();
    }

    return QStringLiteral("%1-count Aida: %2 x %3 in")
        .arg(fabricCount)
        .arg(formatInches(static_cast<double>(imageWidth) / fabricCount),
             formatInches(static_cast<double>(imageHeight) / fabricCount));
}

QImage PatternModel::renderChartPreview(int cellSize, bool drawCenterLines, ChartMode chartMode) const {
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
            const bool drawColor = chartMode != ChartMode::SymbolsOnly;
            const bool drawSymbol = chartMode != ChartMode::ColorsOnly;
            const QColor matchedFill = (matchedIndex >= 0 && matchedIndex < matchedColors.size())
                ? matchedColors[matchedIndex].dmc.color
                : QColor::fromRgba(sprite.rgba);
            const QColor fill = drawColor ? matchedFill : QColor(250, 250, 250);
            painter.fillRect(rect, fill);

            if (drawSymbol) {
                const int luminance = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
                painter.setPen(luminance < 128 ? Qt::white : Qt::black);
                const QString symbol = (matchedIndex >= 0 && matchedIndex < matchedColors.size())
                    ? matchedColors[matchedIndex].symbol
                    : QString();
                painter.drawText(rect, Qt::AlignCenter, symbol);
            }
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

bool PatternModel::writeChartPngFile(const QString &path, int cellSize, QString *errorMessage, ChartMode chartMode) const {
    const QImage chart = renderChartPreview(cellSize, true, chartMode);
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

bool PatternModel::writePdfFile(const QString &path, const QString &imageName, int chartCellSize, QString *errorMessage, ChartMode chartMode) const {
    const QImage chart = renderChartPreview(chartCellSize, true, chartMode);
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
    const int smallGap = pointsToPixels(5, dpi);
    const int contentBottom = content.top() + content.height();
    int y = content.top();

    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(18);
    painter.setFont(titleFont);
    painter.setPen(Qt::black);
    const QString title = imageName.trimmed().isEmpty()
        ? QStringLiteral("Untitled Sprite")
        : imageName.trimmed();
    const int titleHeight = painter.fontMetrics().height();
    painter.drawText(QRect(content.left(), y, content.width(), titleHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     title);
    y += titleHeight + smallGap;

    QFont bodyFont = painter.font();
    bodyFont.setBold(false);
    bodyFont.setPointSize(10);
    painter.setFont(bodyFont);
    const int bodyHeight = painter.fontMetrics().height();
    const QStringList summaryLines{
        QStringLiteral("Pattern size: %1 x %2 stitches").arg(imageWidth).arg(imageHeight),
        QStringLiteral("Total stitch count: %1").arg(opaqueStitchPixels),
        finishedSizeText(14),
        finishedSizeText(16),
        finishedSizeText(18)
    };

    const int summaryPadding = pointsToPixels(7, dpi);
    const int summaryLineGap = pointsToPixels(2, dpi);
    const int summaryHeight = summaryPadding * 2
        + summaryLines.size() * bodyHeight
        + (summaryLines.size() - 1) * summaryLineGap;
    const QRect summaryRect(content.left(), y, content.width(), summaryHeight);
    painter.fillRect(summaryRect, QColor(248, 248, 248));
    painter.setPen(QColor(190, 190, 190));
    painter.drawRect(summaryRect);
    painter.setPen(Qt::black);

    int summaryY = summaryRect.top() + summaryPadding;
    for (const QString &line : summaryLines) {
        painter.drawText(QRect(summaryRect.left() + summaryPadding, summaryY,
                               summaryRect.width() - summaryPadding * 2, bodyHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         line);
        summaryY += bodyHeight + summaryLineGap;
    }
    y = summaryRect.bottom() + gap;

    QFont legendTitleFont = painter.font();
    legendTitleFont.setBold(true);
    legendTitleFont.setPointSize(12);
    QFont legendHeaderFont = painter.font();
    legendHeaderFont.setBold(true);
    legendHeaderFont.setPointSize(9);
    QFont legendRowFont = painter.font();
    legendRowFont.setBold(false);
    legendRowFont.setPointSize(9);
    const int legendTitleHeight = QFontMetrics(legendTitleFont).height();
    const int rowHeight = pointsToPixels(22, dpi);
    const int preferredLegendRows = std::min(std::max(1, static_cast<int>(matchedColors.size())), 8);
    const int legendReservedHeight = legendTitleHeight + smallGap + rowHeight * (preferredLegendRows + 1);

    QSize chartTargetSize = chart.size();
    const int chartMaxHeight = std::max(pointsToPixels(120, dpi),
                                        contentBottom - y - gap - legendReservedHeight);
    chartTargetSize.scale(content.width(), chartMaxHeight, Qt::KeepAspectRatio);
    const QRect chartRect(content.left(), y, chartTargetSize.width(), chartTargetSize.height());
    painter.drawImage(chartRect, chart);
    y = chartRect.bottom() + gap;

    const int tableWidth = content.width();
    const int symbolWidth = tableWidth * 12 / 100;
    const int codeWidth = tableWidth * 16 / 100;
    const int nameWidth = tableWidth * 40 / 100;
    const int countWidth = tableWidth * 16 / 100;
    const int swatchWidth = tableWidth - symbolWidth - codeWidth - nameWidth - countWidth;
    QVector<int> columnWidths{symbolWidth, codeWidth, nameWidth, countWidth, swatchWidth};
    QStringList headers{
        QStringLiteral("Symbol"),
        QStringLiteral("DMC Code"),
        QStringLiteral("DMC Name"),
        QStringLiteral("Stitch Count"),
        QStringLiteral("Swatch")
    };

    auto newPage = [&]() -> bool {
        if (!writer.newPage()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not add a page to the PDF.");
            }
            return false;
        }
        y = content.top();
        return true;
    };

    auto drawRow = [&](const QStringList &values, const QColor &swatchColor, bool header) {
        int x = content.left();
        const QFont previousFont = painter.font();
        if (header) {
            painter.fillRect(QRect(x, y, tableWidth, rowHeight), QColor(235, 235, 235));
            painter.setFont(legendHeaderFont);
        } else {
            painter.setFont(legendRowFont);
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
                int flags = Qt::AlignLeft | Qt::AlignVCenter;
                if (i == 0) {
                    flags = Qt::AlignCenter;
                } else if (i == 3) {
                    flags = Qt::AlignRight | Qt::AlignVCenter;
                }
                drawElidedText(painter, cell, values.value(i), flags);
            }
            x += columnWidths[i];
        }
        painter.setFont(previousFont);
        y += rowHeight;
    };

    auto drawLegendHeader = [&](bool continued) -> bool {
        const int requiredHeight = legendTitleHeight + smallGap + rowHeight;
        if (y + requiredHeight > contentBottom) {
            if (!newPage()) {
                return false;
            }
        }
        painter.setFont(legendTitleFont);
        painter.setPen(Qt::black);
        const QString heading = continued ? QStringLiteral("Legend (continued)") : QStringLiteral("Legend");
        painter.drawText(QRect(content.left(), y, content.width(), legendTitleHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         heading);
        y += legendTitleHeight + smallGap;
        drawRow(headers, QColor(), true);
        return true;
    };

    if (!drawLegendHeader(false)) {
        painter.end();
        return false;
    }

    for (const PatternMatchedColor &matched : matchedColors) {
        if (y + rowHeight > contentBottom) {
            if (!newPage() || !drawLegendHeader(true)) {
                painter.end();
                return false;
            }
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
