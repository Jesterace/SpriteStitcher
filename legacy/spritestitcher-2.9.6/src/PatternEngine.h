#pragma once

#include <QColor>
#include <QImage>
#include <QMap>
#include <QSet>
#include <QObject>
#include <QString>
#include <QVector>

struct DmcColor {
    QString code;
    QString name;
    QColor color;
};

struct PatternColor {
    QString key;
    QString symbol;
    QString dmcCode;
    QString dmcName;
    QColor color;
    int stitches = 0;
};

struct StitchCell {
    int colorIndex = -1;
};

struct PatternOptions {
    enum class BackgroundMode {
        AutoTransparentOrEdge,
        ExactColor,
        StitchEverything
    };

    QString title;
    QString outputDir;
    BackgroundMode backgroundMode = BackgroundMode::AutoTransparentOrEdge;
    QColor backgroundColor = QColor(0, 255, 0);
    bool transparencyAsBackground = true;
    bool matchDmc = true;

    enum class SymbolStyle {
        Classic,
        Clean,
        SimpleIcons
    };
    SymbolStyle symbolStyle = SymbolStyle::Clean;

    enum class ColorCleanupMode {
        None,
        MergeSimilarColors,
        LimitMaxColors
    };

    ColorCleanupMode colorCleanupMode = ColorCleanupMode::None;
    int mergeTolerance = 18;
    int maxColors = 24;

    QMap<QString, QString> dmcOverrides;
    QSet<QString> backgroundColorKeys;
    int removeTinyStitchesMax = 0;

    bool writeColorChart = true;
    bool writeSymbolChart = true;
    enum class GridSize {
        Small,
        Medium,
        Large
    };

    enum class LegendPlacement {
        SeparatePage,
        SamePageWhenPossible
    };

    enum class PageOrientation {
        Portrait,
        Landscape,
        Auto
    };

    bool writeLegendCsv = true;
    bool writePreviewPng = true;
    bool openOutputFolder = false;
    GridSize gridSize = GridSize::Medium;
    LegendPlacement legendPlacement = LegendPlacement::SeparatePage;
    PageOrientation pageOrientation = PageOrientation::Auto;
    bool drawCenterLines = true;
    bool includeCoverPage = false;
};

struct PatternResult {
    bool ok = false;
    QString message;
    QString pdfPath;
    QString csvPath;
    QString patternKeeperPdfPath;
    QString previewPngPath;
    int width = 0;
    int height = 0;
    int stitched = 0;
    int unstitched = 0;
    int colorCount = 0;
    int colorCountBeforeCleanup = 0;
};

struct PatternData {
    PatternResult summary;
    QVector<QVector<StitchCell>> grid;
    QVector<PatternColor> colors;
};

class PatternEngine : public QObject {
    Q_OBJECT
public:
    explicit PatternEngine(QObject *parent = nullptr);

    PatternResult analyzeImage(const QString &imagePath, const PatternOptions &options) const;
    PatternData patternDataForImage(const QString &imagePath, const PatternOptions &options) const;
    QVector<PatternColor> paletteForImage(const QString &imagePath, const PatternOptions &options, PatternResult *summary = nullptr) const;
    PatternResult generatePdf(const QString &imagePath, const PatternOptions &options);
    QVector<DmcColor> dmcPalette() const;

private:
    QVector<DmcColor> m_dmc;

    static QString safeFileBase(QString text);
    static QString makeSymbol(int index, PatternOptions::SymbolStyle style);
    static int colorDistanceSquared(const QColor &a, const QColor &b);
    QColor inferBackgroundColor(const QImage &img) const;
    const DmcColor &nearestDmc(const QColor &color) const;
    const DmcColor *findDmcByCode(const QString &code) const;

    bool shouldSkipPixel(const QColor &pixel, const QColor &inferredBackground, const PatternOptions &options) const;
    void buildPattern(const QImage &img,
                      const PatternOptions &options,
                      QVector<QVector<StitchCell>> &grid,
                      QVector<PatternColor> &colors,
                      int &stitched,
                      int &unstitched,
                      int *colorCountBeforeCleanup = nullptr) const;

    bool renderPdf(const QString &pdfPath,
                   const QString &title,
                   const QVector<QVector<StitchCell>> &grid,
                   const QVector<PatternColor> &colors,
                   int stitched,
                   int unstitched,
                   const PatternOptions &options,
                   QString *error) const;

    bool writeLegendCsv(const QString &csvPath,
                        const QVector<PatternColor> &colors,
                        QString *error) const;

    bool renderPreviewPng(const QString &pngPath,
                          const QVector<QVector<StitchCell>> &grid,
                          const QVector<PatternColor> &colors,
                          QString *error) const;

    bool renderPatternKeeperImportPdf(const QString &pdfPath,
                                      const QString &title,
                                      const QVector<QVector<StitchCell>> &grid,
                                      const QVector<PatternColor> &colors,
                                      int stitched,
                                      int unstitched,
                                      const PatternOptions &options,
                                      QString *error) const;
};
