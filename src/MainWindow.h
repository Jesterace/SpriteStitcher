#pragma once

#include "PatternEngine.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QJsonObject>
#include <QImage>
#include <QLineEdit>
#include <QMap>
#include <QMainWindow>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QTableWidget>
#include <QVariant>

class QResizeEvent;
class QEvent;
class QScrollArea;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void browseImage();
    void chooseOutputFolder();
    void useJesteraceWorkFolders();
    void openSpritesFolder();
    void openPatternsFolder();
    void chooseBackgroundColor();
    void generate();
    void openGeneratedPdf();
    void openOutputFolder();
    void saveProject();
    void loadProject();
    void addBatchFiles();
    void addBatchFolder();
    void clearBatchQueue();
    void generateBatchQueue();
    void reviewPalette();
    void resetSettings();
    void updateBackgroundSwatch();
    void updatePreview();
    void updateSpriteInfo();
    void updateColorCleanupControls();

private:
    void buildUi();
    void loadSettings();
    void saveSettings();
    PatternOptions collectOptions() const;
    QJsonObject projectToJson() const;
    bool applyProjectJson(const QJsonObject &project, QString *errorMessage = nullptr);
    void setComboByData(QComboBox *combo, const QVariant &value, int fallbackIndex = 0);
    QString lastUsefulFolder() const;
    QString paletteSettingsKey(const QString &imagePath) const;
    void loadPaletteOverridesForImage(const QString &imagePath);
    void savePaletteOverridesForCurrentImage();
    void clearPaletteOverridesForCurrentImage();
    QString paletteOverrideSummary() const;
    void addBatchImagePath(const QString &path);
    QStringList batchImagePaths() const;
    void setBatchRowStatus(int row, const QString &status, const QString &detail = QString());
    void logLine(const QString &text);
    void refreshPreviewScale();
    QPixmap buildPreviewPixmap(const QSize &availableSize) const;
    QPixmap buildChartPreviewPixmap(const QSize &availableSize, bool colorChart) const;
    int countSemiTransparentPixels(const QString &imagePath, int *fullyTransparent = nullptr) const;

    PatternEngine m_engine;
    QString m_lastGeneratedFolder;
    QString m_lastGeneratedPdf;
    QString m_currentProjectPath;
    QString m_paletteLoadedForImage;
    QMap<QString, QString> m_dmcOverrides;
    QSet<QString> m_backgroundOverrideKeys;
    int m_removeTinyStitchesMax = 0;

    QLineEdit *m_imageEdit = nullptr;
    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QComboBox *m_backgroundMode = nullptr;
    QPushButton *m_bgColorButton = nullptr;
    QLabel *m_bgSwatch = nullptr;
    QCheckBox *m_dmcCheck = nullptr;
    QCheckBox *m_transparencyUnstitchedCheck = nullptr;
    QComboBox *m_colorCleanupCombo = nullptr;
    QSpinBox *m_mergeToleranceSpin = nullptr;
    QSpinBox *m_maxColorsSpin = nullptr;
    QComboBox *m_chartStyleCombo = nullptr;
    QComboBox *m_symbolStyleCombo = nullptr;
    QComboBox *m_gridSizeCombo = nullptr;
    QComboBox *m_legendPlacementCombo = nullptr;
    QComboBox *m_pageOrientationCombo = nullptr;
    QCheckBox *m_centerLinesCheck = nullptr;
    QCheckBox *m_coverPageCheck = nullptr;
    QCheckBox *m_csvCheck = nullptr;
    QCheckBox *m_previewPngCheck = nullptr;
    QCheckBox *m_openFolderCheck = nullptr;
    QCheckBox *m_openPdfCheck = nullptr;
    QPushButton *m_generateButton = nullptr;
    QPushButton *m_openPdfButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_reviewPaletteButton = nullptr;
    QPushButton *m_addBatchFilesButton = nullptr;
    QPushButton *m_addBatchFolderButton = nullptr;
    QPushButton *m_clearBatchButton = nullptr;
    QPushButton *m_generateBatchButton = nullptr;
    QTableWidget *m_batchTable = nullptr;
    QProgressBar *m_batchProgress = nullptr;
    QLabel *m_batchStatusLabel = nullptr;
    QLabel *m_spriteInfoLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTextEdit *m_log = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_leftSplitter = nullptr;
    QSplitter *m_rightSplitter = nullptr;
    QLabel *m_previewLabel = nullptr;
    QScrollArea *m_previewScrollArea = nullptr;
    QComboBox *m_previewModeCombo = nullptr;
    QComboBox *m_previewScaleCombo = nullptr;
    QCheckBox *m_previewCheckerboardCheck = nullptr;
    QCheckBox *m_previewGridCheck = nullptr;
    QPixmap m_previewPixmap;
    QImage m_previewImage;
    QColor m_bgColor = QColor(0, 255, 0);
};
