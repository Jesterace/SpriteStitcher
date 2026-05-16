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
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QStringList>
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

void drawCenteredSymbol(QPainter &painter, const QRectF &rect, const QString &symbol, const QColor &fill, ChartMode chartMode) {
    if (symbol.isEmpty()) {
        return;
    }

    painter.save();

    QFont font = painter.font();
    font.setBold(true);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);

    int symbolPixelSize = std::max(9, static_cast<int>(std::round(rect.height() * 0.78)));
    font.setPixelSize(symbolPixelSize);

    QFontMetricsF metrics(font);
    while (symbolPixelSize > 6 &&
           (metrics.horizontalAdvance(symbol) > rect.width() * 0.78 ||
            metrics.height() > rect.height() * 0.84)) {
        --symbolPixelSize;
        font.setPixelSize(symbolPixelSize);
        metrics = QFontMetricsF(font);
    }

    QColor textColor(Qt::black);

    if (chartMode == ChartMode::ColorAndSymbols) {
        const int luminance = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        textColor = luminance < 140 ? QColor(Qt::white) : QColor(Qt::black);

        QColor backingColor = textColor == QColor(Qt::white)
            ? QColor(0, 0, 0, 135)
            : QColor(255, 255, 255, 155);

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(backingColor);
        painter.setPen(Qt::NoPen);

        const qreal dotSize = std::min(rect.width(), rect.height()) * 0.56;
        const QRectF dotRect(
            rect.center().x() - dotSize / 2.0,
            rect.center().y() - dotSize / 2.0,
            dotSize,
            dotSize);

        painter.drawEllipse(dotRect);
    }

    painter.setFont(font);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF textBounds = metrics.boundingRect(symbol);
    QPointF textPos(
        rect.left() + (rect.width() - textBounds.width()) / 2 - textBounds.left(),
        rect.top() + (rect.height() - textBounds.height()) / 2 - textBounds.top());

    // Triangles look visually low even when their font bounds are centered.
    // Nudge only triangle glyphs upward so they sit better inside the cell/backing circle.
    if (symbol == QStringLiteral("▲") || symbol == QStringLiteral("△")) {
        textPos.ry() -= rect.height() * 0.09;
    } else if (symbol == QStringLiteral("□")) {
        textPos.ry() -= rect.height() * 0.04;
    }

    QPainterPath path;
    path.addText(textPos, font, symbol);

    if (chartMode == ChartMode::ColorAndSymbols) {
        QPen outlinePen(textColor == QColor(Qt::white) ? QColor(Qt::black) : QColor(Qt::white));
        outlinePen.setWidthF(std::max<qreal>(0.75, rect.width() * 0.035));
        outlinePen.setJoinStyle(Qt::RoundJoin);
        painter.strokePath(path, outlinePen);
    }
    painter.fillPath(path, textColor);

    painter.restore();
}

QColor chartCellFill(const PatternModel &model, const PatternSpriteColor &sprite, bool drawColors) {
    const int matchedIndex = sprite.matchedColorIndex;
    const QColor matchedFill = (matchedIndex >= 0 && matchedIndex < model.matchedColors.size())
        ? model.matchedColors[matchedIndex].dmc.color
        : QColor::fromRgba(sprite.rgba);
    return drawColors ? matchedFill : QColor(250, 250, 250);
}

struct ChartTile {
    int startX = 0;
    int startY = 0;
    int width = 0;
    int height = 0;
    int column = 0;
    int row = 0;
    int columnCount = 0;
    int rowCount = 0;
    int index = 0;
    int count = 0;
};

QString chartTileRangeText(const ChartTile &tile) {
    return QStringLiteral("Columns %1-%2, Rows %3-%4")
        .arg(tile.startX + 1)
        .arg(tile.startX + tile.width)
        .arg(tile.startY + 1)
        .arg(tile.startY + tile.height);
}

void drawChartDirect(QPainter &painter,
                     const PatternModel &model,
                     const QRectF &chartRect,
                     int startX,
                     int startY,
                     int tileWidth,
                     int tileHeight,
                     bool drawCenterLines,
                     ChartMode chartMode) {
    if (chartRect.isEmpty() || model.imageWidth <= 0 || model.imageHeight <= 0 ||
        tileWidth <= 0 || tileHeight <= 0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const bool drawColors = chartMode == ChartMode::ColorAndSymbols || chartMode == ChartMode::ColorsOnly;
    const bool drawSymbols = chartMode == ChartMode::ColorAndSymbols || chartMode == ChartMode::SymbolsOnly;
    const qreal cellWidth = chartRect.width() / tileWidth;
    const qreal cellHeight = chartRect.height() / tileHeight;
    const qreal cellSide = std::min(cellWidth, cellHeight);

    painter.fillRect(chartRect, Qt::white);
    for (int tileY = 0; tileY < tileHeight; ++tileY) {
        const int y = startY + tileY;
        for (int tileX = 0; tileX < tileWidth; ++tileX) {
            const int x = startX + tileX;
            const int index = y * model.imageWidth + x;
            const int spriteIndex = index < model.stitchGrid.size() ? model.stitchGrid[index] : -1;
            if (spriteIndex < 0 || spriteIndex >= model.spriteColors.size()) {
                continue;
            }

            const QRectF cell(
                chartRect.left() + tileX * cellWidth,
                chartRect.top() + tileY * cellHeight,
                cellWidth,
                cellHeight);
            painter.fillRect(cell, chartCellFill(model, model.spriteColors[spriteIndex], drawColors));
        }
    }

    const qreal gridLineWidth = std::max<qreal>(0.5, cellSide * 0.025);
    for (int tileX = 0; tileX <= tileWidth; ++tileX) {
        const int absoluteX = startX + tileX;
        QPen pen((absoluteX % 10 == 0) ? QColor(120, 120, 120) : QColor(210, 210, 210));
        pen.setWidthF(gridLineWidth);
        painter.setPen(pen);
        const qreal px = chartRect.left() + tileX * cellWidth;
        painter.drawLine(QPointF(px, chartRect.top()), QPointF(px, chartRect.bottom()));
    }
    for (int tileY = 0; tileY <= tileHeight; ++tileY) {
        const int absoluteY = startY + tileY;
        QPen pen((absoluteY % 10 == 0) ? QColor(120, 120, 120) : QColor(210, 210, 210));
        pen.setWidthF(gridLineWidth);
        painter.setPen(pen);
        const qreal py = chartRect.top() + tileY * cellHeight;
        painter.drawLine(QPointF(chartRect.left(), py), QPointF(chartRect.right(), py));
    }

    if (drawCenterLines) {
        QPen centerPen(QColor(210, 0, 0));
        centerPen.setWidthF(std::max<qreal>(1.0, cellSide * 0.05));
        painter.setPen(centerPen);
        const int centerXBoundary = model.imageWidth / 2;
        const int centerYBoundary = model.imageHeight / 2;
        if (centerXBoundary >= startX && centerXBoundary <= startX + tileWidth) {
            const qreal centerX = chartRect.left() + (centerXBoundary - startX) * cellWidth;
            painter.drawLine(QPointF(centerX, chartRect.top()), QPointF(centerX, chartRect.bottom()));
        }
        if (centerYBoundary >= startY && centerYBoundary <= startY + tileHeight) {
            const qreal centerY = chartRect.top() + (centerYBoundary - startY) * cellHeight;
            painter.drawLine(QPointF(chartRect.left(), centerY), QPointF(chartRect.right(), centerY));
        }
    }

    if (drawSymbols) {
        for (int tileY = 0; tileY < tileHeight; ++tileY) {
            const int y = startY + tileY;
            for (int tileX = 0; tileX < tileWidth; ++tileX) {
                const int x = startX + tileX;
                const int index = y * model.imageWidth + x;
                const int spriteIndex = index < model.stitchGrid.size() ? model.stitchGrid[index] : -1;
                if (spriteIndex < 0 || spriteIndex >= model.spriteColors.size()) {
                    continue;
                }

                const PatternSpriteColor &sprite = model.spriteColors[spriteIndex];
                const int matchedIndex = sprite.matchedColorIndex;
                const QString symbol = (matchedIndex >= 0 && matchedIndex < model.matchedColors.size())
                    ? model.matchedColors[matchedIndex].symbol
                    : QString();
                const QRectF cell(
                    chartRect.left() + tileX * cellWidth,
                    chartRect.top() + tileY * cellHeight,
                    cellWidth,
                    cellHeight);
                drawCenteredSymbol(painter, cell, symbol, chartCellFill(model, sprite, drawColors), chartMode);
            }
        }
    }

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

bool PatternModel::writePdfFile(const QString &path, const QString &imageName, int chartCellSize, QString *errorMessage, const PdfExportOptions &options, const PdfProgressCallback &progressCallback) const {
    const int pdfChartCellSize = std::max(chartCellSize, 40);
    if (!ok || imageWidth <= 0 || imageHeight <= 0) {
        if (errorMessage) {
            *errorMessage = ok
                ? QStringLiteral("Could not render charts for PDF.")
                : error;
        }
        return false;
    }
    if (!options.includePatternInfo &&
        !options.includeLegend &&
        !options.includeColorOverview &&
        !options.includeBlackAndWhiteSymbolChart &&
        !options.includeTiledColorChart) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Select at least one PDF section to export.");
        }
        return false;
    }
    const int resolvedTileSize =
        (options.tileSize == 50 || options.tileSize == 75 || options.tileSize == 100 || options.tileSize == 125)
            ? options.tileSize
            : 100;
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

    const QRect rawContent = writer.pageLayout().paintRectPixels(dpi);
    const int pageSafetyInset = pointsToPixels(18, dpi);
    const int pageBottomSafetyInset = pointsToPixels(24, dpi);
    const QRect content = rawContent.adjusted(
        pageSafetyInset,
        0,
        -pageSafetyInset,
        -pageBottomSafetyInset);
    const int gap = pointsToPixels(10, dpi);
    const int smallGap = pointsToPixels(5, dpi);
    const int contentBottom = content.top() + content.height();
    const int chartHorizontalInset = pointsToPixels(8, dpi);
    const int chartTopInset = pointsToPixels(8, dpi);
    const int chartBottomInset = pointsToPixels(18, dpi);
    const int legendBottomPadding = pointsToPixels(24, dpi);
    int progressValue = 0;
    int progressMaximum = 1;

    auto cancelPdfExport = [&]() {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PDF export canceled.");
        }
        return false;
    };

    auto reportProgress = [&](const QString &statusText) {
        if (!progressCallback) {
            return true;
        }
        progressValue = std::min(progressValue + 1, progressMaximum);
        if (!progressCallback(progressValue, progressMaximum, statusText)) {
            return cancelPdfExport();
        }
        return true;
    };

    if (progressCallback && !progressCallback(0, progressMaximum, QStringLiteral("Preparing PDF export..."))) {
        painter.end();
        cancelPdfExport();
        return false;
    }

    auto scaledChartSize = [&](int stitchWidth, int stitchHeight, int maxWidth, int maxHeight) {
        const qreal availableWidth = std::max(1, maxWidth);
        const qreal availableHeight = std::max(1, maxHeight);
        QSizeF size(
            static_cast<qreal>(stitchWidth) * pdfChartCellSize,
            static_cast<qreal>(stitchHeight) * pdfChartCellSize);
        size.scale(availableWidth, availableHeight, Qt::KeepAspectRatio);
        return size;
    };

    auto drawPdfHeader = [&](int topY) -> int {
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(18);

        QFont bodyFont = painter.font();
        bodyFont.setBold(false);
        bodyFont.setPointSize(10);

        painter.setFont(titleFont);
        painter.setPen(Qt::black);

        const QString title = imageName.trimmed().isEmpty()
            ? QStringLiteral("Untitled Sprite")
            : imageName.trimmed();

        const int titleHeight = QFontMetrics(titleFont).height();
        painter.drawText(QRect(content.left(), topY, content.width(), titleHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         title);

        const int summaryPadding = pointsToPixels(7, dpi);
        const int summaryLineGap = pointsToPixels(2, dpi);
        const int bodyHeight = QFontMetrics(bodyFont).height();
        const int summaryHeight = summaryPadding * 2
            + 3 * bodyHeight
            + 2 * summaryLineGap;

        const int summaryTop = topY + titleHeight + smallGap;
        const QRect summaryRect(content.left(), summaryTop, content.width(), summaryHeight);

        painter.fillRect(summaryRect, QColor(248, 248, 248));
        painter.setPen(QColor(190, 190, 190));
        painter.drawRect(summaryRect);
        painter.setPen(Qt::black);
        painter.setFont(bodyFont);

        const int rightColumnWidth = summaryRect.width() * 46 / 100;
        const int leftColumnWidth = summaryRect.width() - rightColumnWidth - summaryPadding * 3;

        const QRect leftRect(
            summaryRect.left() + summaryPadding,
            summaryRect.top() + summaryPadding,
            leftColumnWidth,
            summaryRect.height() - summaryPadding * 2);

        const QRect rightRect(
            leftRect.right() + 1 + summaryPadding,
            summaryRect.top() + summaryPadding,
            rightColumnWidth,
            summaryRect.height() - summaryPadding * 2);

        painter.drawText(QRect(leftRect.left(), leftRect.top(),
                               leftRect.width(), bodyHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("Pattern size: %1 x %2 stitches").arg(imageWidth).arg(imageHeight));

        painter.drawText(QRect(leftRect.left(), leftRect.top() + bodyHeight + summaryLineGap,
                               leftRect.width(), bodyHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("Total stitch count: %1").arg(opaqueStitchPixels));

        const QStringList fabricLines{
            finishedSizeText(14),
            finishedSizeText(16),
            finishedSizeText(18)
        };

        int fabricY = rightRect.top();
        for (const QString &line : fabricLines) {
            painter.drawText(QRect(rightRect.left(), fabricY,
                                   rightRect.width(), bodyHeight),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             line);
            fabricY += bodyHeight + summaryLineGap;
        }

        return summaryRect.bottom() + gap;
    };

    const bool useTiledCharts = imageWidth > resolvedTileSize || imageHeight > resolvedTileSize;

    auto drawCoverPage = [&]() {
        const int nextY = drawPdfHeader(content.top());

        QFont coverTitleFont = painter.font();
        coverTitleFont.setBold(true);
        coverTitleFont.setPointSize(14);
        QFont coverBodyFont = painter.font();
        coverBodyFont.setBold(false);
        coverBodyFont.setPointSize(10);

        painter.setFont(coverTitleFont);
        painter.setPen(Qt::black);
        const int titleHeight = QFontMetrics(coverTitleFont).height();
        painter.drawText(QRect(content.left(), nextY, content.width(), titleHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("Pattern Info"));

        painter.setFont(coverBodyFont);
        const int bodyHeight = QFontMetrics(coverBodyFont).height();
        const int lineGap = pointsToPixels(3, dpi);
        int infoY = nextY + titleHeight + smallGap;
        const QStringList infoLines{
            useTiledCharts
                ? QStringLiteral("Large-pattern PDF export uses %1 x %1 stitch chart tiles.").arg(resolvedTileSize)
                : QStringLiteral("Small-pattern PDF export uses full-pattern chart pages."),
            useTiledCharts
                ? QStringLiteral("Grid numbers on tiled chart pages use absolute pattern coordinates.")
                : QStringLiteral("Grid numbers appear every 10 stitches."),
            options.includeColorOverview
                ? QStringLiteral("The color chart is included as a full-pattern overview when it fits on one page.")
                : QStringLiteral("The color overview page is disabled for this export.")
        };

        for (const QString &line : infoLines) {
            painter.drawText(QRect(content.left(), infoY, content.width(), bodyHeight),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             line);
            infoY += bodyHeight + lineGap;
        }
    };

    QFont chartTitleFont = painter.font();
    chartTitleFont.setBold(true);
    chartTitleFont.setPointSize(12);
    const int chartTitleHeight = QFontMetrics(chartTitleFont).height();
    const int chartTitleLineGap = pointsToPixels(2, dpi);

    auto chartTitleBlockHeight = [&](int lineCount) {
        return lineCount * chartTitleHeight + std::max(0, lineCount - 1) * chartTitleLineGap;
    };

    QFont gridNumberFont = painter.font();
    gridNumberFont.setBold(false);
    gridNumberFont.setPointSize(8);
    const QFontMetrics gridNumberMetrics(gridNumberFont);
    const int gridNumberHeight = gridNumberMetrics.height();
    const int lastXMarker = (imageWidth / 10) * 10;
    const int lastYMarker = (imageHeight / 10) * 10;
    const int maxGridMarker = std::max(lastXMarker, lastYMarker);
    const int maxGridNumberWidth = maxGridMarker > 0
        ? gridNumberMetrics.horizontalAdvance(QString::number(maxGridMarker))
        : 0;
    const int gridNumberPadding = pointsToPixels(3, dpi);

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
        return true;
    };

    auto chartSectionArea = [&](const QRect &area) {
        if (area.width() <= chartHorizontalInset * 2 ||
            area.height() <= chartTopInset + chartBottomInset) {
            return QRect();
        }
        return area.adjusted(chartHorizontalInset, chartTopInset, -chartHorizontalInset, -chartBottomInset);
    };

    auto drawChartSection = [&](const QStringList &headingLines,
                                ChartMode chartMode,
                                const QRect &area,
                                int startX,
                                int startY,
                                int stitchWidth,
                                int stitchHeight) {
        const QRect sectionArea = chartSectionArea(area);
        if (sectionArea.isEmpty()) {
            return;
        }

        painter.setFont(chartTitleFont);
        painter.setPen(Qt::black);
        int titleY = sectionArea.top();
        for (const QString &headingLine : headingLines) {
            painter.drawText(QRect(sectionArea.left(), titleY, sectionArea.width(), chartTitleHeight),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             headingLine);
            titleY += chartTitleHeight + chartTitleLineGap;
        }

        const int endX = startX + stitchWidth;
        const int endY = startY + stitchHeight;
        const int firstTopMarker = ((startX / 10) + 1) * 10;
        const int firstLeftMarker = ((startY / 10) + 1) * 10;
        const bool drawTopNumbers = firstTopMarker <= endX;
        const bool drawLeftNumbers = firstLeftMarker <= endY;
        const int topNumberMargin = drawTopNumbers ? gridNumberHeight + gridNumberPadding : 0;
        const int leftNumberMargin = drawLeftNumbers ? maxGridNumberWidth + gridNumberPadding : 0;
        const int chartLeft = sectionArea.left() + leftNumberMargin;
        const int chartTop = sectionArea.top() + chartTitleBlockHeight(headingLines.size()) + smallGap + topNumberMargin;
        const int chartMaxWidth = sectionArea.right() - chartLeft + 1;
        const int chartMaxHeight = sectionArea.bottom() - chartTop + 1;
        if (chartMaxWidth <= 0 || chartMaxHeight <= 0) {
            return;
        }

        const QSizeF targetSize = scaledChartSize(stitchWidth, stitchHeight, chartMaxWidth, chartMaxHeight);
        const QRectF chartRect(chartLeft, chartTop, targetSize.width(), targetSize.height());
        drawChartDirect(painter, *this, chartRect, startX, startY, stitchWidth, stitchHeight, true, chartMode);

        painter.setFont(gridNumberFont);
        painter.setPen(QColor(70, 70, 70));
        if (drawTopNumbers) {
            for (int marker = firstTopMarker; marker <= endX; marker += 10) {
                const QString label = QString::number(marker);
                const int labelWidth = std::max(maxGridNumberWidth, gridNumberMetrics.horizontalAdvance(label));
                const int centerX = static_cast<int>(std::round(chartRect.left() + static_cast<double>(marker - startX) / stitchWidth * chartRect.width()));
                const int labelLeft = std::clamp(
                    centerX - labelWidth / 2,
                    static_cast<int>(std::round(chartRect.left())),
                    std::max(static_cast<int>(std::round(chartRect.left())), static_cast<int>(std::round(chartRect.right())) - labelWidth + 1));
                painter.drawText(
                    QRect(labelLeft, static_cast<int>(std::round(chartRect.top())) - topNumberMargin, labelWidth, gridNumberHeight),
                    Qt::AlignCenter,
                    label);
            }
        }
        if (drawLeftNumbers) {
            for (int marker = firstLeftMarker; marker <= endY; marker += 10) {
                const QString label = QString::number(marker);
                const int centerY = static_cast<int>(std::round(chartRect.top() + static_cast<double>(marker - startY) / stitchHeight * chartRect.height()));
                const int labelTop = std::clamp(
                    centerY - gridNumberHeight / 2,
                    static_cast<int>(std::round(chartRect.top())),
                    std::max(static_cast<int>(std::round(chartRect.top())), static_cast<int>(std::round(chartRect.bottom())) - gridNumberHeight + 1));
                painter.drawText(
                    QRect(static_cast<int>(std::round(chartRect.left())) - leftNumberMargin, labelTop, leftNumberMargin - gridNumberPadding, gridNumberHeight),
                    Qt::AlignRight | Qt::AlignVCenter,
                    label);
            }
        }
    };

    QStringList headers{
        QStringLiteral("Symbol"),
        QStringLiteral("Swatch"),
        QStringLiteral("DMC Code"),
        QStringLiteral("DMC Name"),
        QStringLiteral("Stitch Count")
    };

    auto preferredLegendColumnWidths = [&]() {
        const QFontMetrics headerMetrics(legendHeaderFont);
        const QFontMetrics rowMetrics(legendRowFont);
        const int horizontalPadding = pointsToPixels(8, dpi);
        const int swatchPadding = pointsToPixels(12, dpi);

        int symbolWidth = headerMetrics.horizontalAdvance(headers.value(0)) + horizontalPadding * 2;
        int swatchWidth = headerMetrics.horizontalAdvance(headers.value(1)) + horizontalPadding * 2;
        int codeWidth = headerMetrics.horizontalAdvance(headers.value(2)) + horizontalPadding * 2;
        int nameWidth = headerMetrics.horizontalAdvance(headers.value(3)) + horizontalPadding * 2;
        int countWidth = headerMetrics.horizontalAdvance(headers.value(4)) + horizontalPadding * 2;

        swatchWidth = std::max(swatchWidth, rowHeight + swatchPadding);

        for (const PatternMatchedColor &matched : matchedColors) {
            symbolWidth = std::max(symbolWidth, rowMetrics.horizontalAdvance(matched.symbol) + horizontalPadding * 2);
            codeWidth = std::max(codeWidth, rowMetrics.horizontalAdvance(matched.dmc.number) + horizontalPadding * 2);
            nameWidth = std::max(nameWidth, rowMetrics.horizontalAdvance(matched.dmc.name) + horizontalPadding * 2);
            countWidth = std::max(countWidth, rowMetrics.horizontalAdvance(QString::number(matched.stitchCount)) + horizontalPadding * 2);
        }

        return QVector<int>{symbolWidth, swatchWidth, codeWidth, nameWidth, countWidth};
    };

    auto legendColumnWidths = [&](int maxTableWidth) {
        QVector<int> widths = preferredLegendColumnWidths();

        int totalWidth = 0;
        for (int width : widths) {
            totalWidth += width;
        }

        if (maxTableWidth <= 0 || totalWidth <= maxTableWidth) {
            return widths;
        }

        const int fixedWidth = widths[0] + widths[1] + widths[2] + widths[4];
        const int minNameWidth = QFontMetrics(legendHeaderFont).horizontalAdvance(headers.value(3))
            + pointsToPixels(16, dpi);

        widths[3] = std::max(minNameWidth, maxTableWidth - fixedWidth);
        return widths;
    };

    auto legendTableWidth = [&](int maxTableWidth) {
        const QVector<int> widths = legendColumnWidths(maxTableWidth);
        int totalWidth = 0;
        for (int width : widths) {
            totalWidth += width;
        }
        return std::min(maxTableWidth, totalWidth);
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
        if (rowY + requiredHeight > tableArea.bottom()) {
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
        const QRect actualTableArea(
            tableArea.left(),
            tableArea.top(),
            legendTableWidth(tableArea.width()),
            tableArea.height());

        int legendY = actualTableArea.top();
        if (!drawLegendHeader(actualTableArea, legendY, continued)) {
            return startIndex;
        }

        int legendIndex = startIndex;
        while (legendIndex < matchedColors.size() && legendY + rowHeight <= actualTableArea.bottom()) {
            const PatternMatchedColor &matched = matchedColors[legendIndex];
            drawRow(actualTableArea, legendY, {
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

    auto legendRowCapacity = [&](const QRect &tableArea) {
        const int headerHeight = legendTitleHeight + smallGap + rowHeight;
        if (tableArea.height() < headerHeight) {
            return -1;
        }
        return (tableArea.height() - headerHeight) / rowHeight;
    };

    auto legendFits = [&](const QRect &tableArea) {
        return legendRowCapacity(tableArea) >= matchedColors.size();
    };

    auto chartTargetSizeForArea = [&](const QRect &area, int titleLineCount, int stitchWidth, int stitchHeight) {
        const QRect sectionArea = chartSectionArea(area);
        if (sectionArea.isEmpty()) {
            return QSizeF();
        }

        const int firstTopMarker = 10;
        const int firstLeftMarker = 10;
        const bool drawTopNumbers = firstTopMarker <= stitchWidth;
        const bool drawLeftNumbers = firstLeftMarker <= stitchHeight;
        const int topNumberMargin = drawTopNumbers ? gridNumberHeight + gridNumberPadding : 0;
        const int leftNumberMargin = drawLeftNumbers ? maxGridNumberWidth + gridNumberPadding : 0;
        const int chartLeft = sectionArea.left() + leftNumberMargin;
        const int chartTop = sectionArea.top() + chartTitleBlockHeight(titleLineCount) + smallGap + topNumberMargin;
        const int chartMaxWidth = sectionArea.right() - chartLeft + 1;
        const int chartMaxHeight = sectionArea.bottom() - chartTop + 1;
        if (chartMaxWidth <= 0 || chartMaxHeight <= 0) {
            return QSizeF();
        }
        return scaledChartSize(stitchWidth, stitchHeight, chartMaxWidth, chartMaxHeight);
    };

    auto effectiveCellSize = [&](const QRect &area, int titleLineCount, int stitchWidth, int stitchHeight) {
        const QSizeF targetSize = chartTargetSizeForArea(area, titleLineCount, stitchWidth, stitchHeight);
        if (targetSize.isEmpty() || stitchWidth <= 0 || stitchHeight <= 0) {
            return 0.0;
        }
        return std::min(
            static_cast<double>(targetSize.width()) / stitchWidth,
            static_cast<double>(targetSize.height()) / stitchHeight);
    };

    auto drawLegendPages = [&](int startIndex, bool continued) {
        int legendIndex = startIndex;
        bool drewPage = false;
        while (legendIndex < matchedColors.size() || !drewPage) {
            const QString statusText = (continued || legendIndex > startIndex)
                ? QStringLiteral("Drawing legend (continued)...")
                : QStringLiteral("Drawing legend...");
            if (!reportProgress(statusText)) {
                return false;
            }
            const QRect legendArea(content.left(), content.top(), content.width(), std::max(1, content.height() - legendBottomPadding));
            const int nextLegendIndex = drawLegendRows(legendArea, legendIndex, continued || legendIndex > startIndex);
            drewPage = true;
            if (matchedColors.isEmpty()) {
                return true;
            }
            if (nextLegendIndex == legendIndex) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Could not fit legend rows in the PDF.");
                }
                return false;
            }
            legendIndex = nextLegendIndex;
            if (legendIndex < matchedColors.size() && !newPage()) {
                return false;
            }
        }
        return true;
    };

    const double chartMinReadableCell = pointsToPixels(9, dpi);
    const int minSideLegendWidth = pointsToPixels(185, dpi);
    const int minChartWidth = pointsToPixels(260, dpi);
    const int desiredSideLegendWidth = content.width() * 42 / 100;
    const int fullLegendHeight = legendTitleHeight + smallGap + rowHeight * (matchedColors.size() + 1);

    auto drawChartWithBestLegend = [&](const QString &heading, ChartMode chartMode, int pageTop, double minReadableCell) {
        const QStringList headingLines{heading};
        const QRect pageArea(content.left(), pageTop, content.width(), std::max(1, contentBottom - pageTop - legendBottomPadding));
        if (!options.includeLegend) {
            drawChartSection(headingLines, chartMode, pageArea, 0, 0, imageWidth, imageHeight);
            return true;
        }

        const int maxSideLegendWidth = pageArea.width() - gap - minChartWidth;

        if (maxSideLegendWidth >= minSideLegendWidth) {
            const int sideLegendWidth = std::min(std::max(desiredSideLegendWidth, minSideLegendWidth), maxSideLegendWidth);
            const int chartWidth = pageArea.width() - gap - sideLegendWidth;
            const QRect chartArea(pageArea.left(), pageArea.top(), chartWidth, pageArea.height());
            const QRect legendArea(chartArea.right() + 1 + gap, pageArea.top(), sideLegendWidth, pageArea.height());

            if (effectiveCellSize(chartArea, headingLines.size(), imageWidth, imageHeight) >= minReadableCell && legendFits(legendArea)) {
                drawChartSection(headingLines, chartMode, chartArea, 0, 0, imageWidth, imageHeight);
                drawLegendRows(legendArea, 0, false);
                return true;
            }
        }

        if (fullLegendHeight + gap < pageArea.height()) {
            const int chartHeight = pageArea.height() - gap - fullLegendHeight;
            const QRect chartArea(pageArea.left(), pageArea.top(), pageArea.width(), chartHeight);
            const QRect legendArea(pageArea.left(), chartArea.bottom() + 1 + gap, pageArea.width(), fullLegendHeight);

            if (effectiveCellSize(chartArea, headingLines.size(), imageWidth, imageHeight) >= minReadableCell && legendFits(legendArea)) {
                drawChartSection(headingLines, chartMode, chartArea, 0, 0, imageWidth, imageHeight);
                drawLegendRows(legendArea, 0, false);
                return true;
            }
        }

        drawChartSection(headingLines, chartMode, pageArea, 0, 0, imageWidth, imageHeight);
        if (!newPage()) {
            return false;
        }
        return drawLegendPages(0, false);
    };

    auto chartTiles = [&]() {
        const int tileSize = resolvedTileSize;
        const int columnCount = (imageWidth + tileSize - 1) / tileSize;
        const int rowCount = (imageHeight + tileSize - 1) / tileSize;
        QVector<ChartTile> tiles;
        tiles.reserve(columnCount * rowCount);
        int tileIndex = 0;

        for (int row = 0; row < rowCount; ++row) {
            for (int column = 0; column < columnCount; ++column) {
                const int startX = column * tileSize;
                const int startY = row * tileSize;
                ChartTile tile;
                tile.startX = startX;
                tile.startY = startY;
                tile.width = std::min(tileSize, imageWidth - startX);
                tile.height = std::min(tileSize, imageHeight - startY);
                tile.column = column;
                tile.row = row;
                tile.columnCount = columnCount;
                tile.rowCount = rowCount;
                tile.index = tileIndex++;
                tile.count = columnCount * rowCount;
                tiles.push_back(tile);
            }
        }

        return tiles;
    };

    const QVector<ChartTile> tiles = chartTiles();

    auto drawTiledChart = [&](const QString &heading, ChartMode chartMode) {
        const QRect pageArea(content.left(), content.top(), content.width(), std::max(1, content.height() - legendBottomPadding));

        for (int i = 0; i < tiles.size(); ++i) {
            if (i > 0 && !newPage()) {
                return false;
            }

            const ChartTile &tile = tiles[i];
            if (!reportProgress(QStringLiteral("Drawing %1 tile %2 of %3...")
                    .arg(heading)
                    .arg(tile.index + 1)
                    .arg(tile.count))) {
                return false;
            }

            const QString tileHeading = QStringLiteral("%1 - Tile %2 of %3")
                .arg(heading)
                .arg(tile.index + 1)
                .arg(tile.count);
            const QString rangeText = chartTileRangeText(tile);
            const QString fullHeading = QStringLiteral("%1 - %2").arg(tileHeading, rangeText);
            const int headingWidth = QFontMetrics(chartTitleFont).horizontalAdvance(fullHeading);
            const int headingMaxWidth = std::max(1, pageArea.width() - chartHorizontalInset * 2);
            const QStringList headingLines = headingWidth <= headingMaxWidth
                ? QStringList{fullHeading}
                : QStringList{tileHeading, rangeText};
            drawChartSection(headingLines, chartMode, pageArea, tile.startX, tile.startY, tile.width, tile.height);
        }
        return true;
    };

    auto drawFullPageChart = [&](const QString &heading, ChartMode chartMode) {
        const QRect pageArea(content.left(), content.top(), content.width(), std::max(1, content.height() - legendBottomPadding));
        drawChartSection(QStringList{heading}, chartMode, pageArea, 0, 0, imageWidth, imageHeight);
    };

    const QRect fullChartPageArea(content.left(), content.top(), content.width(), std::max(1, content.height() - legendBottomPadding));
    const double overviewMinCell = pointsToPixels(1.5, dpi);
    const bool drawColorOverview = options.includeColorOverview &&
        effectiveCellSize(fullChartPageArea, 1, imageWidth, imageHeight) >= overviewMinCell;
    const int tilePageCount = static_cast<int>(tiles.size());
    const bool hasRenderableSelection = useTiledCharts
        ? options.includePatternInfo ||
              options.includeLegend ||
              drawColorOverview ||
              options.includeTiledColorChart ||
              options.includeBlackAndWhiteSymbolChart
        : options.includePatternInfo ||
              options.includeLegend ||
              options.includeColorOverview ||
              options.includeTiledColorChart ||
              options.includeBlackAndWhiteSymbolChart;

    if (!hasRenderableSelection) {
        painter.end();
        if (errorMessage) {
            *errorMessage = QStringLiteral("No selected PDF sections can be rendered for this pattern.");
        }
        return false;
    }

    if (useTiledCharts) {
        progressMaximum =
            (options.includePatternInfo ? 1 : 0) +
            (options.includeLegend ? 1 : 0) +
            (drawColorOverview ? 1 : 0) +
            (options.includeTiledColorChart ? tilePageCount : 0) +
            (options.includeBlackAndWhiteSymbolChart ? tilePageCount : 0);
    } else {
        const bool hasFullSmallCharts = options.includeColorOverview || options.includeBlackAndWhiteSymbolChart;
        progressMaximum =
            (options.includePatternInfo && !hasFullSmallCharts ? 1 : 0) +
            (options.includeLegend && !hasFullSmallCharts ? 1 : 0) +
            (options.includeColorOverview ? 1 : 0) +
            (options.includeTiledColorChart ? tilePageCount : 0) +
            (options.includeBlackAndWhiteSymbolChart ? 1 : 0);
    }
    progressMaximum = std::max(1, progressMaximum);

    bool hasPageContent = false;
    auto beginSectionPage = [&]() -> bool {
        if (!hasPageContent) {
            hasPageContent = true;
            return true;
        }
        return newPage();
    };

    if (useTiledCharts) {
        if (options.includePatternInfo) {
            if (!beginSectionPage() ||
                !reportProgress(QStringLiteral("Drawing cover page..."))) {
                painter.end();
                return false;
            }
            drawCoverPage();
        }

        if (options.includeLegend) {
            if (!beginSectionPage() ||
                !drawLegendPages(0, false)) {
                painter.end();
                return false;
            }
        }

        if (drawColorOverview) {
            if (!beginSectionPage() ||
                !reportProgress(QStringLiteral("Drawing color overview..."))) {
                painter.end();
                return false;
            }
            drawFullPageChart(QStringLiteral("Color Overview"), ChartMode::ColorsOnly);
        }

        if (options.includeTiledColorChart) {
            if (!beginSectionPage() ||
                !drawTiledChart(QStringLiteral("Color Chart"), ChartMode::ColorAndSymbols)) {
                painter.end();
                return false;
            }
        }

        if (options.includeBlackAndWhiteSymbolChart) {
            if (!beginSectionPage() ||
                !drawTiledChart(QStringLiteral("Black-and-White Symbol Chart"), ChartMode::SymbolsOnly)) {
                painter.end();
                return false;
            }
        }
    } else {
        const bool hasFullSmallCharts = options.includeColorOverview || options.includeBlackAndWhiteSymbolChart;

        if (options.includePatternInfo && !hasFullSmallCharts) {
            if (!beginSectionPage() ||
                !reportProgress(QStringLiteral("Drawing cover page..."))) {
                painter.end();
                return false;
            }
            drawCoverPage();
        }

        if (options.includeLegend && !hasFullSmallCharts) {
            if (!beginSectionPage() ||
                !drawLegendPages(0, false)) {
                painter.end();
                return false;
            }
        }

        auto drawSmallChart = [&](const QString &heading, ChartMode chartMode, const QString &statusText) {
            if (!beginSectionPage()) {
                return false;
            }
            const int pageTop = options.includePatternInfo
                ? drawPdfHeader(content.top())
                : content.top();
            if (!reportProgress(statusText)) {
                return false;
            }
            return drawChartWithBestLegend(heading, chartMode, pageTop, chartMinReadableCell);
        };

        if (options.includeColorOverview &&
            !drawSmallChart(QStringLiteral("Color Chart"),
                            ChartMode::ColorAndSymbols,
                            QStringLiteral("Drawing color chart..."))) {
            painter.end();
            return false;
        }

        if (options.includeTiledColorChart) {
            if (!beginSectionPage() ||
                !drawTiledChart(QStringLiteral("Color Chart"), ChartMode::ColorAndSymbols)) {
                painter.end();
                return false;
            }
        }

        if (options.includeBlackAndWhiteSymbolChart &&
            !drawSmallChart(QStringLiteral("Black-and-White Symbol Chart"),
                            ChartMode::SymbolsOnly,
                            QStringLiteral("Drawing black-and-white symbol chart..."))) {
            painter.end();
            return false;
        }
    }

    if (progressCallback && !progressCallback(progressMaximum, progressMaximum, QStringLiteral("PDF export complete."))) {
        painter.end();
        cancelPdfExport();
        return false;
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

    static const QStringList symbols{
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
        QStringLiteral("*"),
        QStringLiteral("★"),
        QStringLiteral("☆"),
        QStringLiteral("✚"),
        QStringLiteral("✦"),
        QStringLiteral("✧"),
        QStringLiteral("◐"),
        QStringLiteral("◑"),
        QStringLiteral("◒"),
        QStringLiteral("◓"),
        QStringLiteral("◢"),
        QStringLiteral("◣"),
        QStringLiteral("◤"),
        QStringLiteral("◥"),
        QStringLiteral("◧"),
        QStringLiteral("◨"),
        QStringLiteral("◩"),
        QStringLiteral("◪"),
        QStringLiteral("◫"),
        QStringLiteral("⊕"),
        QStringLiteral("⊗"),
        QStringLiteral("⊙"),
        QStringLiteral("⊡"),
        QStringLiteral("⊞"),
        QStringLiteral("◈"),
        QStringLiteral("▣")
    };

    if (index < symbols.size()) {
        return symbols[index];
    }

    return QStringLiteral("%1%2")
        .arg(symbols[index % symbols.size()])
        .arg(index / symbols.size() + 1);
}
