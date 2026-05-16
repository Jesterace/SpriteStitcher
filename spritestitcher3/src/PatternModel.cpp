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
    int symbolPixelSize = std::max(9, static_cast<int>(std::round(rect.height() * 0.86)));
    font.setPixelSize(symbolPixelSize);
    QFontMetricsF metrics(font);
    while (symbolPixelSize > 6 &&
           (metrics.horizontalAdvance(symbol) > rect.width() * 0.9 ||
            metrics.height() > rect.height() * 0.94)) {
        --symbolPixelSize;
        font.setPixelSize(symbolPixelSize);
        metrics = QFontMetricsF(font);
    }
    painter.setFont(font);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF textBounds = metrics.boundingRect(symbol);
    const QPointF textPos(
        rect.left() + (rect.width() - textBounds.width()) / 2 - textBounds.left(),
        rect.top() + (rect.height() - textBounds.height()) / 2 - textBounds.top());
    QPainterPath path;
    path.addText(textPos, font, symbol);

    QColor textColor(Qt::black);
    QColor outlineColor(Qt::white);
    if (chartMode == ChartMode::ColorAndSymbols) {
        const int luminance = (fill.red() * 299 + fill.green() * 587 + fill.blue() * 114) / 1000;
        textColor = luminance < 140 ? QColor(Qt::white) : QColor(Qt::black);
        outlineColor = textColor == QColor(Qt::white) ? QColor(Qt::black) : QColor(Qt::white);

        QPen outlinePen(outlineColor);
        outlinePen.setWidthF(std::max(0.75, rect.width() * 0.035));
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

void drawChartDirect(QPainter &painter, const PatternModel &model, const QRectF &chartRect, bool drawCenterLines, ChartMode chartMode) {
    if (chartRect.isEmpty() || model.imageWidth <= 0 || model.imageHeight <= 0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const bool drawColors = chartMode == ChartMode::ColorAndSymbols || chartMode == ChartMode::ColorsOnly;
    const bool drawSymbols = chartMode == ChartMode::ColorAndSymbols || chartMode == ChartMode::SymbolsOnly;
    const qreal cellWidth = chartRect.width() / model.imageWidth;
    const qreal cellHeight = chartRect.height() / model.imageHeight;
    const qreal cellSide = std::min(cellWidth, cellHeight);

    painter.fillRect(chartRect, Qt::white);
    for (int y = 0; y < model.imageHeight; ++y) {
        for (int x = 0; x < model.imageWidth; ++x) {
            const int index = y * model.imageWidth + x;
            const int spriteIndex = index < model.stitchGrid.size() ? model.stitchGrid[index] : -1;
            if (spriteIndex < 0 || spriteIndex >= model.spriteColors.size()) {
                continue;
            }

            const QRectF cell(
                chartRect.left() + x * cellWidth,
                chartRect.top() + y * cellHeight,
                cellWidth,
                cellHeight);
            painter.fillRect(cell, chartCellFill(model, model.spriteColors[spriteIndex], drawColors));
        }
    }

    const qreal gridLineWidth = std::max<qreal>(0.5, cellSide * 0.025);
    for (int x = 0; x <= model.imageWidth; ++x) {
        QPen pen((x % 10 == 0) ? QColor(120, 120, 120) : QColor(210, 210, 210));
        pen.setWidthF(gridLineWidth);
        painter.setPen(pen);
        const qreal px = chartRect.left() + x * cellWidth;
        painter.drawLine(QPointF(px, chartRect.top()), QPointF(px, chartRect.bottom()));
    }
    for (int y = 0; y <= model.imageHeight; ++y) {
        QPen pen((y % 10 == 0) ? QColor(120, 120, 120) : QColor(210, 210, 210));
        pen.setWidthF(gridLineWidth);
        painter.setPen(pen);
        const qreal py = chartRect.top() + y * cellHeight;
        painter.drawLine(QPointF(chartRect.left(), py), QPointF(chartRect.right(), py));
    }

    if (drawCenterLines) {
        QPen centerPen(QColor(210, 0, 0));
        centerPen.setWidthF(std::max<qreal>(1.0, cellSide * 0.05));
        painter.setPen(centerPen);
        const qreal centerX = chartRect.left() + (model.imageWidth / 2) * cellWidth;
        const qreal centerY = chartRect.top() + (model.imageHeight / 2) * cellHeight;
        painter.drawLine(QPointF(centerX, chartRect.top()), QPointF(centerX, chartRect.bottom()));
        painter.drawLine(QPointF(chartRect.left(), centerY), QPointF(chartRect.right(), centerY));
    }

    if (drawSymbols) {
        for (int y = 0; y < model.imageHeight; ++y) {
            for (int x = 0; x < model.imageWidth; ++x) {
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
                    chartRect.left() + x * cellWidth,
                    chartRect.top() + y * cellHeight,
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

bool PatternModel::writePdfFile(const QString &path, const QString &imageName, int chartCellSize, QString *errorMessage) const {
    const int pdfChartCellSize = std::max(chartCellSize, 40);
    if (!ok || imageWidth <= 0 || imageHeight <= 0) {
        if (errorMessage) {
            *errorMessage = ok
                ? QStringLiteral("Could not render charts for PDF.")
                : error;
        }
        return false;
    }
    const QSizeF chartSourceSize(
        static_cast<qreal>(imageWidth) * pdfChartCellSize,
        static_cast<qreal>(imageHeight) * pdfChartCellSize);

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
    const int chartHorizontalInset = pointsToPixels(8, dpi);
    const int chartTopInset = pointsToPixels(8, dpi);
    const int chartBottomInset = pointsToPixels(18, dpi);
    int y = content.top();

    auto scaledChartSize = [&](int maxWidth, int maxHeight) {
        const qreal availableWidth = std::max(1, maxWidth);
        const qreal availableHeight = std::max(1, maxHeight);
        QSizeF size = chartSourceSize;
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

    y = drawPdfHeader(content.top());

    QFont chartTitleFont = painter.font();
    chartTitleFont.setBold(true);
    chartTitleFont.setPointSize(12);
    const int chartTitleHeight = QFontMetrics(chartTitleFont).height();

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
        y = content.top();
        return true;
    };

    auto chartSectionArea = [&](const QRect &area) {
        if (area.width() <= chartHorizontalInset * 2 ||
            area.height() <= chartTopInset + chartBottomInset) {
            return QRect();
        }
        return area.adjusted(chartHorizontalInset, chartTopInset, -chartHorizontalInset, -chartBottomInset);
    };

    auto drawChartSection = [&](const QString &heading, ChartMode chartMode, const QRect &area) {
        const QRect sectionArea = chartSectionArea(area);
        if (sectionArea.isEmpty()) {
            return;
        }

        painter.setFont(chartTitleFont);
        painter.setPen(Qt::black);
        painter.drawText(QRect(sectionArea.left(), sectionArea.top(), sectionArea.width(), chartTitleHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         heading);

        const bool drawTopNumbers = imageWidth >= 10;
        const bool drawLeftNumbers = imageHeight >= 10;
        const int topNumberMargin = drawTopNumbers ? gridNumberHeight + gridNumberPadding : 0;
        const int leftNumberMargin = drawLeftNumbers ? maxGridNumberWidth + gridNumberPadding : 0;
        const int chartLeft = sectionArea.left() + leftNumberMargin;
        const int chartTop = sectionArea.top() + chartTitleHeight + smallGap + topNumberMargin;
        const int chartMaxWidth = sectionArea.right() - chartLeft + 1;
        const int chartMaxHeight = sectionArea.bottom() - chartTop + 1;
        if (chartMaxWidth <= 0 || chartMaxHeight <= 0) {
            return;
        }

        const QSizeF targetSize = scaledChartSize(chartMaxWidth, chartMaxHeight);
        const QRectF chartRect(chartLeft, chartTop, targetSize.width(), targetSize.height());
        drawChartDirect(painter, *this, chartRect, true, chartMode);

        painter.setFont(gridNumberFont);
        painter.setPen(QColor(70, 70, 70));
        if (drawTopNumbers) {
            for (int marker = 10; marker <= imageWidth; marker += 10) {
                const QString label = QString::number(marker);
                const int labelWidth = std::max(maxGridNumberWidth, gridNumberMetrics.horizontalAdvance(label));
                const int centerX = static_cast<int>(std::round(chartRect.left() + static_cast<double>(marker) / imageWidth * chartRect.width()));
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
            for (int marker = 10; marker <= imageHeight; marker += 10) {
                const QString label = QString::number(marker);
                const int centerY = static_cast<int>(std::round(chartRect.top() + static_cast<double>(marker) / imageHeight * chartRect.height()));
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

    auto chartTargetSizeForArea = [&](const QRect &area) {
        const QRect sectionArea = chartSectionArea(area);
        if (sectionArea.isEmpty()) {
            return QSizeF();
        }

        const bool drawTopNumbers = imageWidth >= 10;
        const bool drawLeftNumbers = imageHeight >= 10;
        const int topNumberMargin = drawTopNumbers ? gridNumberHeight + gridNumberPadding : 0;
        const int leftNumberMargin = drawLeftNumbers ? maxGridNumberWidth + gridNumberPadding : 0;
        const int chartLeft = sectionArea.left() + leftNumberMargin;
        const int chartTop = sectionArea.top() + chartTitleHeight + smallGap + topNumberMargin;
        const int chartMaxWidth = sectionArea.right() - chartLeft + 1;
        const int chartMaxHeight = sectionArea.bottom() - chartTop + 1;
        if (chartMaxWidth <= 0 || chartMaxHeight <= 0) {
            return QSizeF();
        }
        return scaledChartSize(chartMaxWidth, chartMaxHeight);
    };

    auto effectiveCellSize = [&](const QRect &area) {
        const QSizeF targetSize = chartTargetSizeForArea(area);
        if (targetSize.isEmpty() || imageWidth <= 0 || imageHeight <= 0) {
            return 0.0;
        }
        return std::min(
            static_cast<double>(targetSize.width()) / imageWidth,
            static_cast<double>(targetSize.height()) / imageHeight);
    };

    auto drawLegendPages = [&](int startIndex, bool continued) {
        int legendIndex = startIndex;
        bool drewPage = false;
        while (legendIndex < matchedColors.size() || !drewPage) {
            const QRect legendArea(content.left(), content.top(), content.width(), content.height());
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
        const QRect pageArea(content.left(), pageTop, content.width(), contentBottom - pageTop);
        const int maxSideLegendWidth = pageArea.width() - gap - minChartWidth;

        if (maxSideLegendWidth >= minSideLegendWidth) {
            const int sideLegendWidth = std::min(std::max(desiredSideLegendWidth, minSideLegendWidth), maxSideLegendWidth);
            const int chartWidth = pageArea.width() - gap - sideLegendWidth;
            const QRect chartArea(pageArea.left(), pageArea.top(), chartWidth, pageArea.height());
            const QRect legendArea(chartArea.right() + 1 + gap, pageArea.top(), sideLegendWidth, pageArea.height());

            if (effectiveCellSize(chartArea) >= minReadableCell && legendFits(legendArea)) {
                drawChartSection(heading, chartMode, chartArea);
                drawLegendRows(legendArea, 0, false);
                return true;
            }
        }

        if (fullLegendHeight + gap < pageArea.height()) {
            const int chartHeight = pageArea.height() - gap - fullLegendHeight;
            const QRect chartArea(pageArea.left(), pageArea.top(), pageArea.width(), chartHeight);
            const QRect legendArea(pageArea.left(), chartArea.bottom() + 1 + gap, pageArea.width(), fullLegendHeight);

            if (effectiveCellSize(chartArea) >= minReadableCell && legendFits(legendArea)) {
                drawChartSection(heading, chartMode, chartArea);
                drawLegendRows(legendArea, 0, false);
                return true;
            }
        }

        drawChartSection(heading, chartMode, pageArea);
        if (!newPage()) {
            return false;
        }
        return drawLegendPages(0, false);
    };

    if (!drawChartWithBestLegend(QStringLiteral("Color Chart"), ChartMode::ColorAndSymbols, y, chartMinReadableCell)) {
        painter.end();
        return false;
    }

    if (!newPage()) {
        painter.end();
        return false;
    }

    y = drawPdfHeader(content.top());

    if (!drawChartWithBestLegend(QStringLiteral("Black-and-White Symbol Chart"), ChartMode::SymbolsOnly, y, chartMinReadableCell)) {
        painter.end();
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
