#include "PatternModel.h"

#include <QFile>
#include <QHash>
#include <QFont>
#include <QImageWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
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

void drawCenteredSymbol(QPainter &painter, const QRect &rect, const QString &symbol, const QColor &fill, ChartMode chartMode) {
    Q_UNUSED(fill);
    if (symbol.isEmpty()) {
        return;
    }

    painter.save();

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(std::max(7, static_cast<int>(rect.height() * 0.58)));
    painter.setFont(font);

    QFontMetrics metrics(font);
    const QRect textBounds = metrics.boundingRect(symbol);
    QRect backingRect(
        rect.left() + (rect.width() - textBounds.width()) / 2 - 3,
        rect.top() + (rect.height() - textBounds.height()) / 2 - 3,
        textBounds.width() + 6,
        textBounds.height() + 6);

    backingRect = backingRect.intersected(rect.adjusted(1, 1, -1, -1));

    if (chartMode == ChartMode::ColorAndSymbols) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 220));
        painter.drawRoundedRect(backingRect, 3, 3);
    }

    painter.setPen(Qt::black);
    painter.setBrush(Qt::NoBrush);
    painter.drawText(rect, Qt::AlignCenter, symbol);

    painter.restore();
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

    const bool drawColors = chartMode == ChartMode::ColorAndSymbols || chartMode == ChartMode::ColorsOnly;
    const bool drawSymbols = chartMode == ChartMode::ColorAndSymbols || chartMode == ChartMode::SymbolsOnly;

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
            const QColor matchedFill = (matchedIndex >= 0 && matchedIndex < matchedColors.size())
                ? matchedColors[matchedIndex].dmc.color
                : QColor::fromRgba(sprite.rgba);
            const QColor fill = drawColors ? matchedFill : QColor(250, 250, 250);
            painter.fillRect(rect, fill);
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

    if (drawSymbols) {
        for (int y = 0; y < imageHeight; ++y) {
            for (int x = 0; x < imageWidth; ++x) {
                const int index = y * imageWidth + x;
                const int spriteIndex = index < stitchGrid.size() ? stitchGrid[index] : -1;
                if (spriteIndex < 0 || spriteIndex >= spriteColors.size()) {
                    continue;
                }

                const PatternSpriteColor &sprite = spriteColors[spriteIndex];
                const int matchedIndex = sprite.matchedColorIndex;
                const QString symbol = (matchedIndex >= 0 && matchedIndex < matchedColors.size())
                    ? matchedColors[matchedIndex].symbol
                    : QString();
                const QColor fill = drawColors
                    ? ((matchedIndex >= 0 && matchedIndex < matchedColors.size())
                        ? matchedColors[matchedIndex].dmc.color
                        : QColor::fromRgba(sprite.rgba))
                    : QColor(250, 250, 250);
                drawCenteredSymbol(painter, QRect(x * cellSize, y * cellSize, cellSize, cellSize), symbol, fill, chartMode);
            }
        }
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

bool PatternModel::writePdfFile(const QString &path, const QString &imageName, int chartCellSize, QString *errorMessage) const {
    const QImage colorChart = renderChartPreview(chartCellSize, true, ChartMode::ColorsOnly);
    const QImage symbolChart = renderChartPreview(chartCellSize, true, ChartMode::SymbolsOnly);
    if (colorChart.isNull() || symbolChart.isNull()) {
        if (errorMessage) {
            *errorMessage = ok
                ? QStringLiteral("Could not render charts for PDF.")
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

    auto scaledChartSize = [&](const QImage &chart, int maxWidth, int maxHeight) {
        QSize size = chart.size();
        size.scale(std::max(1, maxWidth), std::max(1, maxHeight), Qt::KeepAspectRatio);
        return size;
    };

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

    QFont chartTitleFont = painter.font();
    chartTitleFont.setBold(true);
    chartTitleFont.setPointSize(12);
    const int chartTitleHeight = QFontMetrics(chartTitleFont).height();

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

    auto drawChartSection = [&](const QString &heading, const QImage &chart, const QRect &area) {
        painter.setFont(chartTitleFont);
        painter.setPen(Qt::black);
        painter.drawText(QRect(area.left(), area.top(), area.width(), chartTitleHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         heading);

        const int chartTop = area.top() + chartTitleHeight + smallGap;
        const int chartMaxHeight = area.bottom() - chartTop + 1;
        const QSize targetSize = scaledChartSize(chart, area.width(), chartMaxHeight);
        const QRect chartRect(area.left(), chartTop, targetSize.width(), targetSize.height());
        painter.drawImage(chartRect, chart);
    };

    auto legendColumnWidths = [](int tableWidth) {
        const int symbolWidth = tableWidth * 12 / 100;
        const int swatchWidth = tableWidth * 14 / 100;
        const int codeWidth = tableWidth * 16 / 100;
        const int countWidth = tableWidth * 16 / 100;
        const int nameWidth = tableWidth - symbolWidth - swatchWidth - codeWidth - countWidth;
        return QVector<int>{symbolWidth, swatchWidth, codeWidth, nameWidth, countWidth};
    };

    QStringList headers{
        QStringLiteral("Symbol"),
        QStringLiteral("Swatch"),
        QStringLiteral("DMC Code"),
        QStringLiteral("DMC Name"),
        QStringLiteral("Stitch Count")
    };

    auto drawRow = [&](const QRect &tableArea, int &rowY, const QStringList &values, const QColor &swatchColor, bool header) {
        int x = tableArea.left();
        const int tableWidth = tableArea.width();
        const QVector<int> columnWidths = legendColumnWidths(tableWidth);
        const QFont previousFont = painter.font();
        if (header) {
            painter.fillRect(QRect(x, rowY, tableWidth, rowHeight), QColor(235, 235, 235));
            painter.setFont(legendHeaderFont);
        } else {
            painter.setFont(legendRowFont);
        }
        painter.setPen(QColor(170, 170, 170));
        painter.drawRect(QRect(x, rowY, tableWidth, rowHeight));
        for (int i = 0; i < columnWidths.size(); ++i) {
            const QRect cell(x, rowY, columnWidths[i], rowHeight);
            painter.setPen(QColor(170, 170, 170));
            painter.drawRect(cell);
            painter.setPen(Qt::black);
            if (!header && i == 1) {
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
                } else if (i == 4) {
                    flags = Qt::AlignRight | Qt::AlignVCenter;
                }
                drawElidedText(painter, cell, values.value(i), flags);
            }
            x += columnWidths[i];
        }
        painter.setFont(previousFont);
        rowY += rowHeight;
    };

    auto drawLegendHeader = [&](const QRect &tableArea, int &rowY, bool continued) -> bool {
        const int requiredHeight = legendTitleHeight + smallGap + rowHeight;
        if (rowY + requiredHeight > tableArea.bottom() + 1) {
            return false;
        }
        painter.setFont(legendTitleFont);
        painter.setPen(Qt::black);
        const QString heading = continued ? QStringLiteral("Legend (continued)") : QStringLiteral("Legend");
        painter.drawText(QRect(tableArea.left(), rowY, tableArea.width(), legendTitleHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         heading);
        rowY += legendTitleHeight + smallGap;
        drawRow(tableArea, rowY, headers, QColor(), true);
        return true;
    };

    auto drawLegendRows = [&](const QRect &tableArea, int startIndex, bool continued) {
        int legendY = tableArea.top();
        if (!drawLegendHeader(tableArea, legendY, continued)) {
            return startIndex;
        }

        int legendIndex = startIndex;
        while (legendIndex < matchedColors.size() && legendY + rowHeight <= tableArea.bottom() + 1) {
            const PatternMatchedColor &matched = matchedColors[legendIndex];
            drawRow(tableArea, legendY, {
                matched.symbol,
                QString(),
                matched.dmc.number,
                matched.dmc.name,
                QString::number(matched.stitchCount)
            }, matched.dmc.color, false);
            ++legendIndex;
        }
        return legendIndex;
    };

    const int firstPageTop = y;
    const int minSideLegendWidth = pointsToPixels(185, dpi);
    const int minChartWidth = pointsToPixels(260, dpi);
    const int desiredSideLegendWidth = content.width() * 42 / 100;
    const int maxSideLegendWidth = content.width() - gap - minChartWidth;
    const bool sideBySideAvailable = maxSideLegendWidth >= minSideLegendWidth;
    int continuationStart = 0;

    if (sideBySideAvailable) {
        const int sideLegendWidth = std::min(std::max(desiredSideLegendWidth, minSideLegendWidth), maxSideLegendWidth);
        const int chartWidth = content.width() - gap - sideLegendWidth;
        const QRect chartArea(content.left(), firstPageTop, chartWidth, contentBottom - firstPageTop);
        const QRect legendArea(chartArea.right() + 1 + gap, firstPageTop, sideLegendWidth, contentBottom - firstPageTop);

        drawChartSection(QStringLiteral("Color Chart"), colorChart, chartArea);
        continuationStart = std::max(continuationStart, drawLegendRows(legendArea, 0, false));
    } else {
        const QRect chartArea(content.left(), firstPageTop, content.width(), contentBottom - firstPageTop);
        drawChartSection(QStringLiteral("Color Chart"), colorChart, chartArea);
    }

    if (!newPage()) {
        painter.end();
        return false;
    }

    if (sideBySideAvailable) {
        const int sideLegendWidth = std::min(std::max(desiredSideLegendWidth, minSideLegendWidth), maxSideLegendWidth);
        const int chartWidth = content.width() - gap - sideLegendWidth;
        const QRect chartArea(content.left(), content.top(), chartWidth, content.height());
        const QRect legendArea(chartArea.right() + 1 + gap, content.top(), sideLegendWidth, content.height());

        drawChartSection(QStringLiteral("Black-and-White Symbol Chart"), symbolChart, chartArea);
        continuationStart = std::max(continuationStart, drawLegendRows(legendArea, 0, false));
    } else {
        const QRect chartArea(content.left(), content.top(), content.width(), content.height());
        drawChartSection(QStringLiteral("Black-and-White Symbol Chart"), symbolChart, chartArea);
    }

    int legendIndex = continuationStart;
    while (legendIndex < matchedColors.size()) {
        if (!newPage()) {
            painter.end();
            return false;
        }

        const QRect legendArea(content.left(), content.top(), content.width(), content.height());
        const int nextLegendIndex = drawLegendRows(legendArea, legendIndex, legendIndex > 0);
        if (nextLegendIndex == legendIndex) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not fit legend rows in the PDF.");
            }
            painter.end();
            return false;
        }
        legendIndex = nextLegendIndex;
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
