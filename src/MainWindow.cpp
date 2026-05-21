#include "MainWindow.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QColorDialog>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QFormLayout>
#include <QFontMetrics>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSettings>
#include <QSplitter>
#include <QStringList>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString dmcSortKey(const QString &code) {
    bool ok = false;
    const int number = code.toInt(&ok);
    if (ok) return QStringLiteral("0_%1").arg(number, 5, 10, QChar('0'));
    if (code.compare(QStringLiteral("B5200"), Qt::CaseInsensitive) == 0) return QStringLiteral("1_05200");
    if (code.compare(QStringLiteral("White"), Qt::CaseInsensitive) == 0) return QStringLiteral("1_05201");
    return QStringLiteral("2_") + code.toUpper();
}

class DmcTableItem : public QTableWidgetItem {
public:
    explicit DmcTableItem(const QString &text) : QTableWidgetItem(text) {
        setData(Qt::UserRole, dmcSortKey(text));
    }

    bool operator<(const QTableWidgetItem &other) const override {
        return data(Qt::UserRole).toString() < other.data(Qt::UserRole).toString();
    }
};

QTableWidgetItem *readOnlyItem(const QString &text) {
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QTableWidgetItem *readOnlyNumberItem(int value) {
    auto *item = new QTableWidgetItem;
    item->setData(Qt::DisplayRole, value);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QString("SpriteStitcher v%1").arg(APP_VERSION));
    setWindowIcon(QIcon(":/spritestitcher.svg"));
    buildUi();
    loadSettings();
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);

    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    m_mainSplitter->setChildrenCollapsible(false);

    auto *leftPanel = new QWidget;
    leftPanel->setMinimumWidth(420);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_leftSplitter = new QSplitter(Qt::Vertical, leftPanel);
    m_leftSplitter->setChildrenCollapsible(false);
    leftLayout->addWidget(m_leftSplitter, 1);

    auto makeScrollPanel = [](QWidget *widget) {
        auto *scroll = new QScrollArea;
        scroll->setWidget(widget);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        return scroll;
    };

    auto *topPanel = new QWidget;
    auto *topLayout = new QVBoxLayout(topPanel);
    topLayout->setContentsMargins(0, 0, 0, 0);

    auto *inputBox = new QGroupBox("Input", topPanel);
    auto *inputLayout = new QFormLayout(inputBox);

    auto *imageRow = new QWidget(inputBox);
    auto *imageLayout = new QHBoxLayout(imageRow);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    m_imageEdit = new QLineEdit(imageRow);
    auto *browseButton = new QPushButton("Browse...", imageRow);
    imageLayout->addWidget(m_imageEdit, 1);
    imageLayout->addWidget(browseButton);
    inputLayout->addRow("Sprite PNG:", imageRow);

    m_titleEdit = new QLineEdit(inputBox);
    m_titleEdit->setPlaceholderText("Example: Sonic 3 Sprite");
    inputLayout->addRow("Pattern title:", m_titleEdit);

    auto *outputRow = new QWidget(inputBox);
    auto *outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    m_outputEdit = new QLineEdit(outputRow);
    auto *outputButton = new QPushButton("Choose...", outputRow);
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(outputButton);
    inputLayout->addRow("Output folder:", outputRow);

    topLayout->addWidget(inputBox);

    auto *optionsBox = new QGroupBox("Pattern options", topPanel);
    auto *optionsBoxLayout = new QVBoxLayout(optionsBox);
    optionsBoxLayout->setContentsMargins(8, 8, 8, 8);
    optionsBoxLayout->setSpacing(6);

    auto *optionsTabs = new QTabWidget(optionsBox);
    optionsBoxLayout->addWidget(optionsTabs);

    auto createOptionsTab = [&](const QString &title) -> QFormLayout* {
        auto *tab = new QWidget(optionsTabs);
        auto *layout = new QFormLayout(tab);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
        optionsTabs->addTab(tab, title);
        return layout;
    };

    auto *patternOptionsLayout = createOptionsTab("Pattern");
    auto *chartPdfOptionsLayout = createOptionsTab("Chart / PDF");
    auto *outputOptionsLayout = createOptionsTab("Output");
    auto *patternOptionsTab = patternOptionsLayout->parentWidget();
    auto *chartPdfOptionsTab = chartPdfOptionsLayout->parentWidget();
    auto *outputOptionsTab = outputOptionsLayout->parentWidget();

    m_backgroundMode = new QComboBox(patternOptionsTab);
    m_backgroundMode->addItem("Auto: transparent or edge color is unstitched", static_cast<int>(PatternOptions::BackgroundMode::AutoTransparentOrEdge));
    m_backgroundMode->addItem("Exact background color is unstitched", static_cast<int>(PatternOptions::BackgroundMode::ExactColor));
    m_backgroundMode->addItem("Stitch every visible pixel", static_cast<int>(PatternOptions::BackgroundMode::StitchEverything));
    patternOptionsLayout->addRow("Background:", m_backgroundMode);

    auto *bgRow = new QWidget(patternOptionsTab);
    auto *bgLayout = new QHBoxLayout(bgRow);
    bgLayout->setContentsMargins(0, 0, 0, 0);
    m_bgSwatch = new QLabel(bgRow);
    m_bgSwatch->setFixedSize(42, 24);
    m_bgSwatch->setFrameShape(QFrame::Panel);
    m_bgSwatch->setFrameShadow(QFrame::Sunken);
    m_bgColorButton = new QPushButton("Pick color...", bgRow);
    bgLayout->addWidget(m_bgSwatch);
    bgLayout->addWidget(m_bgColorButton);
    bgLayout->addStretch(1);
    patternOptionsLayout->addRow("Exact BG color:", bgRow);

    m_transparencyUnstitchedCheck = new QCheckBox("Treat transparent pixels as unstitched background", patternOptionsTab);
    m_transparencyUnstitchedCheck->setChecked(true);
    m_transparencyUnstitchedCheck->setToolTip("Recommended for sprites. If unchecked, fully transparent pixels can be interpreted as stitches using their stored RGB values.");
    patternOptionsLayout->addRow(m_transparencyUnstitchedCheck);

    m_dmcCheck = new QCheckBox("Match sprite colors to nearest DMC floss", patternOptionsTab);
    m_dmcCheck->setChecked(true);
    patternOptionsLayout->addRow(m_dmcCheck);

    m_colorCleanupCombo = new QComboBox(patternOptionsTab);
    m_colorCleanupCombo->addItem("Exact/nearest DMC only", static_cast<int>(PatternOptions::ColorCleanupMode::None));
    m_colorCleanupCombo->addItem("Merge very similar colors", static_cast<int>(PatternOptions::ColorCleanupMode::MergeSimilarColors));
    m_colorCleanupCombo->addItem("Limit max colors", static_cast<int>(PatternOptions::ColorCleanupMode::LimitMaxColors));
    patternOptionsLayout->addRow("Color cleanup:", m_colorCleanupCombo);

    m_mergeToleranceSpin = new QSpinBox(patternOptionsTab);
    m_mergeToleranceSpin->setRange(0, 80);
    m_mergeToleranceSpin->setValue(18);
    m_mergeToleranceSpin->setSuffix(" tolerance");
    m_mergeToleranceSpin->setToolTip("Higher values merge more near-identical shades. Start around 12–24 for anti-aliased sprites.");
    patternOptionsLayout->addRow("Merge strength:", m_mergeToleranceSpin);

    m_maxColorsSpin = new QSpinBox(patternOptionsTab);
    m_maxColorsSpin->setRange(2, 80);
    m_maxColorsSpin->setValue(24);
    m_maxColorsSpin->setSuffix(" colors");
    m_maxColorsSpin->setToolTip("When limiting colors, the most-used colors are kept and rare colors are remapped to the nearest kept color.");
    patternOptionsLayout->addRow("Max colors:", m_maxColorsSpin);

    m_chartStyleCombo = new QComboBox(chartPdfOptionsTab);
    m_chartStyleCombo->addItem("Both: color chart + Pattern Keeper symbol chart", QStringLiteral("both"));
    m_chartStyleCombo->addItem("Color chart only", QStringLiteral("color"));
    m_chartStyleCombo->addItem("Pattern Keeper symbol chart only", QStringLiteral("symbols"));
    chartPdfOptionsLayout->addRow("Chart style:", m_chartStyleCombo);

    m_symbolStyleCombo = new QComboBox(chartPdfOptionsTab);
    m_symbolStyleCombo->addItem("Clean cross-stitch symbols", static_cast<int>(PatternOptions::SymbolStyle::Clean));
    m_symbolStyleCombo->addItem("Simple black icon symbols", static_cast<int>(PatternOptions::SymbolStyle::SimpleIcons));
    m_symbolStyleCombo->addItem("Classic SpriteStitch symbols", static_cast<int>(PatternOptions::SymbolStyle::Classic));
    m_symbolStyleCombo->setToolTip("Choose the symbol family used in chart previews, PDFs, and the legend. The clean and icon styles are inspired by simple cross-stitch chart symbols.");
    chartPdfOptionsLayout->addRow("Symbol set:", m_symbolStyleCombo);

    m_gridSizeCombo = new QComboBox(chartPdfOptionsTab);
    m_gridSizeCombo->addItem("Small", static_cast<int>(PatternOptions::GridSize::Small));
    m_gridSizeCombo->addItem("Medium", static_cast<int>(PatternOptions::GridSize::Medium));
    m_gridSizeCombo->addItem("Large", static_cast<int>(PatternOptions::GridSize::Large));
    m_gridSizeCombo->setCurrentIndex(1);
    chartPdfOptionsLayout->addRow("Grid size:", m_gridSizeCombo);

    m_legendPlacementCombo = new QComboBox(chartPdfOptionsTab);
    m_legendPlacementCombo->addItem("Separate legend page", static_cast<int>(PatternOptions::LegendPlacement::SeparatePage));
    m_legendPlacementCombo->addItem("Same page when possible", static_cast<int>(PatternOptions::LegendPlacement::SamePageWhenPossible));
    chartPdfOptionsLayout->addRow("Legend:", m_legendPlacementCombo);

    m_pageOrientationCombo = new QComboBox(chartPdfOptionsTab);
    m_pageOrientationCombo->addItem("Auto", static_cast<int>(PatternOptions::PageOrientation::Auto));
    m_pageOrientationCombo->addItem("Portrait", static_cast<int>(PatternOptions::PageOrientation::Portrait));
    m_pageOrientationCombo->addItem("Landscape", static_cast<int>(PatternOptions::PageOrientation::Landscape));
    chartPdfOptionsLayout->addRow("Page orientation:", m_pageOrientationCombo);

    m_csvCheck = new QCheckBox("Write CSV legend/shopping list", outputOptionsTab);
    m_csvCheck->setChecked(true);
    m_previewPngCheck = new QCheckBox("Write website PNG preview", outputOptionsTab);
    m_previewPngCheck->setChecked(true);
    m_previewPngCheck->setToolTip("Creates a transparent-background PNG from the final stitched pattern grid for website previews.");
    m_centerLinesCheck = new QCheckBox("Draw red center lines", chartPdfOptionsTab);
    m_centerLinesCheck->setChecked(true);
    m_coverPageCheck = new QCheckBox("Include cover page", chartPdfOptionsTab);
    m_openFolderCheck = new QCheckBox("Open output folder after generating", outputOptionsTab);
    m_openPdfCheck = new QCheckBox("Open generated PDF after generating", outputOptionsTab);

    chartPdfOptionsLayout->addRow(m_centerLinesCheck);
    chartPdfOptionsLayout->addRow(m_coverPageCheck);
    outputOptionsLayout->addRow(m_csvCheck);
    outputOptionsLayout->addRow(m_previewPngCheck);
    outputOptionsLayout->addRow(m_openPdfCheck);
    outputOptionsLayout->addRow(m_openFolderCheck);

    topLayout->addWidget(optionsBox);

    auto *actionsBox = new QGroupBox("Actions", topPanel);
    auto *actionsLayout = new QGridLayout(actionsBox);
    actionsLayout->setContentsMargins(8, 8, 8, 8);
    actionsLayout->setHorizontalSpacing(8);
    actionsLayout->setVerticalSpacing(8);

    auto addActionSection = [&](const QString &title, int row, int column, int columnSpan) -> QGridLayout* {
        auto *section = new QWidget(actionsBox);
        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(4);

        auto *heading = new QLabel(title, section);
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        heading->setFont(headingFont);
        sectionLayout->addWidget(heading);

        auto *buttonLayout = new QGridLayout;
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setHorizontalSpacing(6);
        buttonLayout->setVerticalSpacing(6);
        sectionLayout->addLayout(buttonLayout);

        actionsLayout->addWidget(section, row, column, 1, columnSpan);
        return buttonLayout;
    };

    auto *mainActionLayout = addActionSection("Main action", 0, 0, 2);
    auto *projectLayout = addActionSection("Project", 1, 0, 1);
    auto *foldersLayout = addActionSection("Folders", 1, 1, 1);
    auto *toolsLayout = addActionSection("Tools", 2, 0, 2);

    m_generateButton = new QPushButton("Generate PDF", actionsBox);
    m_openPdfButton = new QPushButton("Open PDF", actionsBox);
    m_openPdfButton->setEnabled(false);
    m_openFolderButton = new QPushButton("Open folder", actionsBox);
    m_reviewPaletteButton = new QPushButton("Edit palette...", actionsBox);
    auto *saveProjectButton = new QPushButton("Save project", actionsBox);
    auto *loadProjectButton = new QPushButton("Load project", actionsBox);
    auto *useWorkFoldersButton = new QPushButton("Use work folders", actionsBox);
    auto *openSpritesButton = new QPushButton("Open sprites", actionsBox);
    auto *openPatternsButton = new QPushButton("Open patterns", actionsBox);
    auto *resetButton = new QPushButton("Reset settings", actionsBox);

    QList<QPushButton*> actionButtons = {
        m_generateButton,
        m_openPdfButton,
        m_openFolderButton,
        m_reviewPaletteButton,
        saveProjectButton,
        loadProjectButton,
        useWorkFoldersButton,
        openSpritesButton,
        openPatternsButton,
        resetButton
    };

    for (auto *button : actionButtons) {
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    m_generateButton->setMinimumHeight(34);

    mainActionLayout->addWidget(m_generateButton, 0, 0, 1, 2);
    mainActionLayout->addWidget(m_openPdfButton, 1, 0);
    mainActionLayout->addWidget(m_openFolderButton, 1, 1);
    mainActionLayout->setColumnStretch(0, 1);
    mainActionLayout->setColumnStretch(1, 1);

    projectLayout->addWidget(saveProjectButton, 0, 0);
    projectLayout->addWidget(loadProjectButton, 0, 1);
    projectLayout->setColumnStretch(0, 1);
    projectLayout->setColumnStretch(1, 1);

    foldersLayout->addWidget(useWorkFoldersButton, 0, 0, 1, 2);
    foldersLayout->addWidget(openSpritesButton, 1, 0);
    foldersLayout->addWidget(openPatternsButton, 1, 1);
    foldersLayout->setColumnStretch(0, 1);
    foldersLayout->setColumnStretch(1, 1);

    toolsLayout->addWidget(m_reviewPaletteButton, 0, 0);
    toolsLayout->addWidget(resetButton, 0, 1);
    toolsLayout->setColumnStretch(0, 1);
    toolsLayout->setColumnStretch(1, 1);

    actionsLayout->setColumnStretch(0, 1);
    actionsLayout->setColumnStretch(1, 1);
    topLayout->addWidget(actionsBox);
    topLayout->addStretch(1);

    auto *topScroll = makeScrollPanel(topPanel);
    topScroll->setMinimumHeight(220);
    m_leftSplitter->addWidget(topScroll);

    auto *summaryBox = new QGroupBox("Generation summary");
    auto *summaryLayout = new QVBoxLayout(summaryBox);
    m_summaryLabel = new QLabel("No pattern generated yet.", summaryBox);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_summaryLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    summaryLayout->addWidget(m_summaryLabel);

    auto *summaryScroll = makeScrollPanel(summaryBox);
    summaryScroll->setMinimumHeight(90);
    m_leftSplitter->addWidget(summaryScroll);

    auto *logBox = new QGroupBox("Log");
    auto *logLayout = new QVBoxLayout(logBox);
    m_log = new QTextEdit(logBox);
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(80);
    logLayout->addWidget(m_log, 1);
    m_leftSplitter->addWidget(logBox);

    m_leftSplitter->setStretchFactor(0, 5);
    m_leftSplitter->setStretchFactor(1, 1);
    m_leftSplitter->setStretchFactor(2, 2);
    m_leftSplitter->setSizes({430, 120, 170});

    auto *leftScroll = new QScrollArea(m_mainSplitter);
    leftScroll->setWidget(leftPanel);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    leftScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    leftScroll->setMinimumWidth(260);

    m_rightSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
    m_rightSplitter->setChildrenCollapsible(false);
    m_rightSplitter->setMinimumWidth(320);

    auto *infoPanel = new QWidget;
    auto *infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(0, 0, 0, 0);

    auto *spriteInfoBox = new QGroupBox("Sprite info", infoPanel);
    auto *spriteInfoLayout = new QVBoxLayout(spriteInfoBox);
    m_spriteInfoLabel = new QLabel("Choose a sprite PNG to see size, stitch count, color count, and finished-size estimates.", spriteInfoBox);
    m_spriteInfoLabel->setWordWrap(true);
    m_spriteInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_spriteInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_spriteInfoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    spriteInfoLayout->addWidget(m_spriteInfoLabel);
    infoLayout->addWidget(spriteInfoBox, 1);

    auto *infoScroll = new QScrollArea(m_rightSplitter);
    infoScroll->setWidget(infoPanel);
    infoScroll->setWidgetResizable(true);
    infoScroll->setFrameShape(QFrame::NoFrame);
    infoScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    infoScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    infoScroll->setMinimumHeight(150);

    auto *previewBox = new QGroupBox("Full image preview", m_rightSplitter);
    previewBox->setMinimumHeight(220);
    auto *previewLayout = new QVBoxLayout(previewBox);

    auto *previewControls = new QWidget(previewBox);
    auto *previewControlsLayout = new QHBoxLayout(previewControls);
    previewControlsLayout->setContentsMargins(0, 0, 0, 0);
    previewControlsLayout->addWidget(new QLabel("Preview:", previewControls));
    m_previewModeCombo = new QComboBox(previewControls);
    m_previewModeCombo->addItem("Sprite image", QStringLiteral("sprite"));
    m_previewModeCombo->addItem("Chart: color + symbols", QStringLiteral("chart_color"));
    m_previewModeCombo->addItem("Chart: symbols only", QStringLiteral("chart_symbols"));
    m_previewModeCombo->setToolTip("Switch between the raw sprite and an in-app preview of the generated chart grid.");
    previewControlsLayout->addWidget(m_previewModeCombo);

    previewControlsLayout->addWidget(new QLabel("Zoom:", previewControls));
    m_previewScaleCombo = new QComboBox(previewControls);
    m_previewScaleCombo->addItem("Fit to window", QStringLiteral("fit"));
    m_previewScaleCombo->addItem("Actual size (1x)", QStringLiteral("1"));
    m_previewScaleCombo->addItem("2x", QStringLiteral("2"));
    m_previewScaleCombo->addItem("4x", QStringLiteral("4"));
    m_previewScaleCombo->addItem("8x", QStringLiteral("8"));
    previewControlsLayout->addWidget(m_previewScaleCombo);
    m_previewCheckerboardCheck = new QCheckBox("Checkerboard transparency", previewControls);
    m_previewCheckerboardCheck->setChecked(true);
    previewControlsLayout->addWidget(m_previewCheckerboardCheck);
    m_previewGridCheck = new QCheckBox("Pixel grid", previewControls);
    m_previewGridCheck->setToolTip("Draws a pixel grid on the preview when zoomed in enough.");
    previewControlsLayout->addWidget(m_previewGridCheck);
    previewControlsLayout->addStretch(1);
    previewLayout->addWidget(previewControls);

    m_previewLabel = new QLabel("Choose a sprite PNG to preview it here.", previewBox);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(300, 220);
    m_previewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_previewLabel->setFrameShape(QFrame::StyledPanel);
    m_previewLabel->setWordWrap(true);

    m_previewScrollArea = new QScrollArea(previewBox);
    m_previewScrollArea->setWidget(m_previewLabel);
    m_previewScrollArea->setWidgetResizable(false);
    m_previewScrollArea->setAlignment(Qt::AlignCenter);
    m_previewScrollArea->setFrameShape(QFrame::NoFrame);
    m_previewScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_previewScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_previewScrollArea->viewport()->installEventFilter(this);
    previewLayout->addWidget(m_previewScrollArea, 1);

    m_rightSplitter->addWidget(infoScroll);
    m_rightSplitter->addWidget(previewBox);
    m_rightSplitter->setStretchFactor(0, 1);
    m_rightSplitter->setStretchFactor(1, 3);
    m_rightSplitter->setSizes({220, 500});

    m_mainSplitter->addWidget(leftScroll);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 3);
    m_mainSplitter->setSizes({560, 600});

    root->addWidget(m_mainSplitter, 1);

    setCentralWidget(central);
    setMinimumSize(640, 500);
    resize(1160, 720);

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseImage);
    connect(outputButton, &QPushButton::clicked, this, &MainWindow::chooseOutputFolder);
    connect(m_bgColorButton, &QPushButton::clicked, this, &MainWindow::chooseBackgroundColor);
    connect(m_generateButton, &QPushButton::clicked, this, &MainWindow::generate);
    connect(m_openPdfButton, &QPushButton::clicked, this, &MainWindow::openGeneratedPdf);
    connect(m_openFolderButton, &QPushButton::clicked, this, &MainWindow::openOutputFolder);
    connect(saveProjectButton, &QPushButton::clicked, this, &MainWindow::saveProject);
    connect(loadProjectButton, &QPushButton::clicked, this, &MainWindow::loadProject);
    connect(useWorkFoldersButton, &QPushButton::clicked, this, &MainWindow::useJesteraceWorkFolders);
    connect(openSpritesButton, &QPushButton::clicked, this, &MainWindow::openSpritesFolder);
    connect(openPatternsButton, &QPushButton::clicked, this, &MainWindow::openPatternsFolder);
    connect(m_reviewPaletteButton, &QPushButton::clicked, this, &MainWindow::reviewPalette);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetSettings);
    connect(m_backgroundMode, &QComboBox::currentIndexChanged, this, &MainWindow::updateBackgroundSwatch);
    connect(m_imageEdit, &QLineEdit::textChanged, this, &MainWindow::updatePreview);
    connect(m_dmcCheck, &QCheckBox::toggled, this, [this] { updateSpriteInfo(); refreshPreviewScale(); });
    connect(m_transparencyUnstitchedCheck, &QCheckBox::toggled, this, [this] { updateSpriteInfo(); refreshPreviewScale(); });
    connect(m_backgroundMode, &QComboBox::currentIndexChanged, this, [this] { updateSpriteInfo(); refreshPreviewScale(); });
    connect(m_colorCleanupCombo, &QComboBox::currentIndexChanged, this, &MainWindow::updateColorCleanupControls);
    connect(m_previewModeCombo, &QComboBox::currentIndexChanged, this, [this] { refreshPreviewScale(); });
    connect(m_previewScaleCombo, &QComboBox::currentIndexChanged, this, [this] { refreshPreviewScale(); });
    connect(m_previewCheckerboardCheck, &QCheckBox::toggled, this, [this] { refreshPreviewScale(); });
    connect(m_previewGridCheck, &QCheckBox::toggled, this, [this] { refreshPreviewScale(); });
    connect(m_symbolStyleCombo, &QComboBox::currentIndexChanged, this, [this] { refreshPreviewScale(); });
    connect(m_centerLinesCheck, &QCheckBox::toggled, this, [this] { refreshPreviewScale(); });
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, [this] { refreshPreviewScale(); });
    connect(m_rightSplitter, &QSplitter::splitterMoved, this, [this] { refreshPreviewScale(); });
    connect(m_mergeToleranceSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updateSpriteInfo(); refreshPreviewScale(); });
    connect(m_maxColorsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this] { updateSpriteInfo(); refreshPreviewScale(); });

    updateBackgroundSwatch();
    updateColorCleanupControls();
    updatePreview();
    logLine(QString("Ready. SpriteStitcher v%1. Pattern Keeper import PDFs, website preview PNGs, CSV legends, and work-folder shortcuts are ready.").arg(APP_VERSION));
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    refreshPreviewScale();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (m_previewScrollArea && watched == m_previewScrollArea->viewport() && event->type() == QEvent::Resize) {
        refreshPreviewScale();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updatePreview() {
    if (!m_previewLabel) return;

    const QString path = m_imageEdit->text().trimmed();
    loadPaletteOverridesForImage(path);
    if (path.isEmpty()) {
        m_previewPixmap = QPixmap();
        m_previewImage = QImage();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText("Choose a sprite PNG to preview it here.");
        m_previewLabel->setMinimumSize(300, 220);
        m_previewLabel->resize(300, 220);
        updateSpriteInfo();
        return;
    }

    QPixmap pixmap(path);
    if (pixmap.isNull()) {
        m_previewPixmap = QPixmap();
        m_previewImage = QImage();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText("Preview unavailable. Make sure the selected file is an image.");
        m_previewLabel->setMinimumSize(300, 220);
        m_previewLabel->resize(300, 220);
        updateSpriteInfo();
        return;
    }

    m_previewPixmap = pixmap;
    m_previewImage = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    m_previewLabel->setText(QString());
    refreshPreviewScale();
    updateSpriteInfo();
}


void MainWindow::updateSpriteInfo() {
    if (!m_spriteInfoLabel) return;

    const QString path = m_imageEdit ? m_imageEdit->text().trimmed() : QString();
    if (path.isEmpty()) {
        m_spriteInfoLabel->setText("Choose a sprite PNG to see size, stitch count, color count, and finished-size estimates.");
        return;
    }

    PatternResult info = m_engine.analyzeImage(path, collectOptions());
    if (!info.ok) {
        m_spriteInfoLabel->setText("Sprite info unavailable. Make sure the selected file is a valid image.");
        return;
    }

    const int total = info.width * info.height;
    QStringList lines;
    lines << QString("Grid: %1 × %2 stitches").arg(info.width).arg(info.height);
    lines << QString("Total squares: %1").arg(total);
    lines << QString("Stitched squares: %1").arg(info.stitched);
    lines << QString("Unstitched/background squares: %1").arg(info.unstitched);
    if (info.colorCountBeforeCleanup > 0 && info.colorCountBeforeCleanup != info.colorCount) {
        lines << QString("Colors: %1 after cleanup/overrides (%2 before cleanup)").arg(info.colorCount).arg(info.colorCountBeforeCleanup);
    } else {
        lines << QString("Colors: %1").arg(info.colorCount);
    }
    int fullyTransparent = 0;
    const int semiTransparent = countSemiTransparentPixels(path, &fullyTransparent);
    if (fullyTransparent > 0 || semiTransparent > 0) {
        lines << QString("Transparent pixels: %1 fully transparent, %2 semi-transparent").arg(fullyTransparent).arg(semiTransparent);
    }
    if (!m_dmcOverrides.isEmpty() || !m_backgroundOverrideKeys.isEmpty() || m_removeTinyStitchesMax > 0) {
        lines << QString("Palette overrides: %1").arg(paletteOverrideSummary());
    }
    lines << QString("Finished size estimates:");
    lines << QString("  14 count: %1 × %2 in").arg(info.width / 14.0, 0, 'f', 2).arg(info.height / 14.0, 0, 'f', 2);
    lines << QString("  16 count: %1 × %2 in").arg(info.width / 16.0, 0, 'f', 2).arg(info.height / 16.0, 0, 'f', 2);
    lines << QString("  18 count: %1 × %2 in").arg(info.width / 18.0, 0, 'f', 2).arg(info.height / 18.0, 0, 'f', 2);

    if (total >= 25000 || info.stitched >= 25000) {
        lines << "";
        lines << "Warning: large pattern. PDF generation may take longer and the finished piece will be sizeable.";
    }
    const int warningColorCount = std::max(info.colorCount, info.colorCountBeforeCleanup);
    if (warningColorCount >= 50) {
        lines << "";
        if (info.colorCountBeforeCleanup > 0 && info.colorCountBeforeCleanup != info.colorCount) {
            lines << QString("Notice: cleanup reduced the color count from %1 to %2.").arg(info.colorCountBeforeCleanup).arg(info.colorCount);
        } else {
            lines << "Warning: high color count. If this was not intentional, try Merge very similar colors or Limit max colors.";
        }
    }
    if (semiTransparent > 0) {
        lines << "";
        lines << "Warning: semi-transparent edge pixels were found. They may become stitches or background depending on the transparency option.";
    }

    m_spriteInfoLabel->setText(lines.join('\n'));
}

void MainWindow::updateColorCleanupControls() {
    if (!m_colorCleanupCombo || !m_mergeToleranceSpin || !m_maxColorsSpin) return;
    const auto mode = static_cast<PatternOptions::ColorCleanupMode>(m_colorCleanupCombo->currentData().toInt());
    m_mergeToleranceSpin->setEnabled(mode == PatternOptions::ColorCleanupMode::MergeSimilarColors);
    m_maxColorsSpin->setEnabled(mode == PatternOptions::ColorCleanupMode::LimitMaxColors);
    updateSpriteInfo();
    refreshPreviewScale();
}

void MainWindow::refreshPreviewScale() {
    if (!m_previewLabel || m_previewImage.isNull()) return;

    QSize available(320, 240);
    if (m_previewScrollArea && m_previewScrollArea->viewport()) {
        available = m_previewScrollArea->viewport()->size();
    }
    if (available.width() <= 0 || available.height() <= 0) return;

    const QPixmap rendered = buildPreviewPixmap(available);
    if (rendered.isNull()) return;

    m_previewLabel->setText(QString());
    m_previewLabel->setPixmap(rendered);
    const QString mode = m_previewScaleCombo ? m_previewScaleCombo->currentData().toString() : QStringLiteral("fit");
    const QSize labelSize = mode == QStringLiteral("fit") ? available : rendered.size();
    m_previewLabel->setMinimumSize(labelSize);
    m_previewLabel->resize(labelSize);
}

QPixmap MainWindow::buildPreviewPixmap(const QSize &availableSize) const {
    if (m_previewImage.isNull()) return QPixmap();

    const QString previewMode = m_previewModeCombo ? m_previewModeCombo->currentData().toString() : QStringLiteral("sprite");
    if (previewMode == QStringLiteral("chart_color")) {
        return buildChartPreviewPixmap(availableSize, true);
    }
    if (previewMode == QStringLiteral("chart_symbols")) {
        return buildChartPreviewPixmap(availableSize, false);
    }

    const QString mode = m_previewScaleCombo ? m_previewScaleCombo->currentData().toString() : QStringLiteral("fit");
    QSize outputSize;

    if (mode == QStringLiteral("fit")) {
        const QSize boundedSize = availableSize.expandedTo(QSize(1, 1));
        const double widthScale = boundedSize.width() / static_cast<double>(std::max(1, m_previewImage.width()));
        const double heightScale = boundedSize.height() / static_cast<double>(std::max(1, m_previewImage.height()));
        const double scale = std::min(widthScale, heightScale);
        outputSize = QSize(std::max(1, qRound(m_previewImage.width() * scale)),
                           std::max(1, qRound(m_previewImage.height() * scale)));
    } else {
        bool ok = false;
        const int factor = std::max(1, mode.toInt(&ok));
        outputSize = m_previewImage.size() * (ok ? factor : 1);
    }

    outputSize.setWidth(std::max(1, outputSize.width()));
    outputSize.setHeight(std::max(1, outputSize.height()));

    QImage canvas(outputSize, QImage::Format_RGB32);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const bool checkerboard = m_previewCheckerboardCheck ? m_previewCheckerboardCheck->isChecked() : true;
    if (checkerboard) {
        const int square = 12;
        const QColor light(230, 230, 230);
        const QColor dark(190, 190, 190);
        for (int y = 0; y < outputSize.height(); y += square) {
            for (int x = 0; x < outputSize.width(); x += square) {
                const bool odd = ((x / square) + (y / square)) % 2;
                painter.fillRect(QRect(x, y, square, square), odd ? dark : light);
            }
        }
    } else {
        canvas.fill(Qt::white);
    }

    const QImage scaled = m_previewImage.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    painter.drawImage(0, 0, scaled);

    const bool drawGrid = m_previewGridCheck && m_previewGridCheck->isChecked();
    if (drawGrid && m_previewImage.width() > 0 && m_previewImage.height() > 0) {
        const double cellW = outputSize.width() / static_cast<double>(m_previewImage.width());
        const double cellH = outputSize.height() / static_cast<double>(m_previewImage.height());
        if (cellW >= 4.0 && cellH >= 4.0) {
            QPen gridPen(QColor(90, 90, 90, 150));
            gridPen.setWidth(1);
            painter.setPen(gridPen);
            for (int x = 0; x <= m_previewImage.width(); ++x) {
                const int px = qRound(x * cellW);
                painter.drawLine(px, 0, px, outputSize.height());
            }
            for (int y = 0; y <= m_previewImage.height(); ++y) {
                const int py = qRound(y * cellH);
                painter.drawLine(0, py, outputSize.width(), py);
            }
        }
    }

    painter.end();
    return QPixmap::fromImage(canvas);
}

QPixmap MainWindow::buildChartPreviewPixmap(const QSize &availableSize, bool colorChart) const {
    const QString imagePath = m_imageEdit ? m_imageEdit->text().trimmed() : QString();
    if (imagePath.isEmpty()) return QPixmap();

    const PatternData data = m_engine.patternDataForImage(imagePath, collectOptions());
    if (!data.summary.ok || data.grid.isEmpty() || data.summary.width <= 0 || data.summary.height <= 0) {
        return QPixmap();
    }

    const int cols = data.summary.width;
    const int rows = data.summary.height;
    const QString mode = m_previewScaleCombo ? m_previewScaleCombo->currentData().toString() : QStringLiteral("fit");

    int cell = 14;
    if (mode == QStringLiteral("fit")) {
        cell = std::max(1, std::min(availableSize.width() / std::max(1, cols),
                                   availableSize.height() / std::max(1, rows)));
    } else {
        bool ok = false;
        const int factor = std::max(1, mode.toInt(&ok));
        switch (ok ? factor : 1) {
            case 1: cell = 14; break;
            case 2: cell = 22; break;
            case 4: cell = 34; break;
            case 8: cell = 50; break;
            default: cell = 14; break;
        }
    }

    const int maxPreviewDim = 5200;
    if (cols * cell > maxPreviewDim || rows * cell > maxPreviewDim) {
        cell = std::max(1, std::min(maxPreviewDim / std::max(1, cols), maxPreviewDim / std::max(1, rows)));
    }

    const QSize outputSize(std::max(1, cols * cell), std::max(1, rows * cell));
    QImage canvas(outputSize, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QFont symbolFont = painter.font();
    symbolFont.setBold(true);
    symbolFont.setPixelSize(std::max(6, static_cast<int>(cell * 0.52)));
    painter.setFont(symbolFont);

    const bool drawSymbols = cell >= 8;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const int colorIndex = data.grid[y][x].colorIndex;
            const QRect rect(x * cell, y * cell, cell, cell);
            if (colorIndex >= 0 && colorIndex < data.colors.size()) {
                const PatternColor &pc = data.colors[colorIndex];
                painter.fillRect(rect, colorChart ? pc.color : QColor(Qt::white));
                if (drawSymbols) {
                    const int luminance = (pc.color.red() * 299 + pc.color.green() * 587 + pc.color.blue() * 114) / 1000;
                    painter.setPen((colorChart && luminance < 120) ? Qt::white : Qt::black);
                    painter.drawText(rect.adjusted(1, 0, -1, 0), Qt::AlignCenter, pc.symbol);
                }
            } else {
                painter.fillRect(rect, Qt::white);
                if (cell >= 10) {
                    painter.setPen(QColor(235, 235, 235));
                    painter.drawLine(rect.topLeft(), rect.bottomRight());
                }
            }
        }
    }

    const bool drawGrid = m_previewGridCheck ? m_previewGridCheck->isChecked() : true;
    if (drawGrid || cell >= 8) {
        for (int x = 0; x <= cols; ++x) {
            QPen pen((x % 10 == 0) ? QColor(90, 90, 90) : QColor(190, 190, 190));
            pen.setWidth((x % 10 == 0 && cell >= 4) ? 2 : 1);
            painter.setPen(pen);
            painter.drawLine(x * cell, 0, x * cell, outputSize.height());
        }
        for (int y = 0; y <= rows; ++y) {
            QPen pen((y % 10 == 0) ? QColor(90, 90, 90) : QColor(190, 190, 190));
            pen.setWidth((y % 10 == 0 && cell >= 4) ? 2 : 1);
            painter.setPen(pen);
            painter.drawLine(0, y * cell, outputSize.width(), y * cell);
        }
    }

    if (m_centerLinesCheck && m_centerLinesCheck->isChecked()) {
        QPen centerPen(QColor(210, 0, 0));
        centerPen.setWidth(std::max(2, cell / 7));
        painter.setPen(centerPen);
        const int centerX = (cols / 2) * cell;
        const int centerY = (rows / 2) * cell;
        painter.drawLine(centerX, 0, centerX, outputSize.height());
        painter.drawLine(0, centerY, outputSize.width(), centerY);
    }

    painter.end();
    return QPixmap::fromImage(canvas);
}

int MainWindow::countSemiTransparentPixels(const QString &imagePath, int *fullyTransparent) const {
    if (fullyTransparent) *fullyTransparent = 0;

    QImage image(imagePath);
    if (image.isNull()) return 0;

    image = image.convertToFormat(QImage::Format_ARGB32);
    int semiTransparent = 0;
    int transparent = 0;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha == 0) {
                ++transparent;
            } else if (alpha < 255) {
                ++semiTransparent;
            }
        }
    }

    if (fullyTransparent) *fullyTransparent = transparent;
    return semiTransparent;
}

QString MainWindow::lastUsefulFolder() const {
    if (!m_outputEdit->text().trimmed().isEmpty()) return m_outputEdit->text().trimmed();
    if (!m_imageEdit->text().trimmed().isEmpty()) return QFileInfo(m_imageEdit->text().trimmed()).absolutePath();
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return pictures.isEmpty() ? QDir::homePath() : pictures;
}

QString MainWindow::paletteSettingsKey(const QString &imagePath) const {
    const QByteArray bytes = QFileInfo(imagePath).absoluteFilePath().toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}

void MainWindow::loadPaletteOverridesForImage(const QString &imagePath) {
    const QString trimmed = imagePath.trimmed();
    if (trimmed == m_paletteLoadedForImage) return;

    m_dmcOverrides.clear();
    m_backgroundOverrideKeys.clear();
    m_removeTinyStitchesMax = 0;
    m_paletteLoadedForImage = trimmed;

    if (trimmed.isEmpty()) return;

    QSettings settings("JesteraceTools", "SpriteStitchCPP");
    settings.beginGroup(QStringLiteral("paletteOverrides"));
    settings.beginGroup(paletteSettingsKey(trimmed));

    const QStringList dmcPairs = settings.value(QStringLiteral("dmcOverrides")).toStringList();
    for (const QString &pair : dmcPairs) {
        const int tab = pair.indexOf('\t');
        if (tab > 0) {
            const QString key = pair.left(tab);
            const QString code = pair.mid(tab + 1).trimmed();
            if (!key.isEmpty() && !code.isEmpty()) m_dmcOverrides.insert(key, code);
        }
    }

    const QStringList backgroundKeys = settings.value(QStringLiteral("backgroundKeys")).toStringList();
    for (const QString &key : backgroundKeys) {
        if (!key.isEmpty()) m_backgroundOverrideKeys.insert(key);
    }
    m_removeTinyStitchesMax = settings.value(QStringLiteral("removeTinyStitchesMax"), 0).toInt();

    settings.endGroup();
    settings.endGroup();
}

void MainWindow::savePaletteOverridesForCurrentImage() {
    const QString imagePath = m_imageEdit ? m_imageEdit->text().trimmed() : QString();
    if (imagePath.isEmpty()) return;

    QSettings settings("JesteraceTools", "SpriteStitchCPP");
    settings.beginGroup(QStringLiteral("paletteOverrides"));
    settings.beginGroup(paletteSettingsKey(imagePath));

    QStringList dmcPairs;
    for (auto it = m_dmcOverrides.constBegin(); it != m_dmcOverrides.constEnd(); ++it) {
        if (!it.key().isEmpty() && !it.value().trimmed().isEmpty()) {
            dmcPairs << (it.key() + QLatin1Char('\t') + it.value().trimmed());
        }
    }
    settings.setValue(QStringLiteral("dmcOverrides"), dmcPairs);

    QStringList backgroundKeys;
    for (const QString &key : m_backgroundOverrideKeys) {
        if (!key.isEmpty()) backgroundKeys << key;
    }
    backgroundKeys.sort(Qt::CaseInsensitive);
    settings.setValue(QStringLiteral("backgroundKeys"), backgroundKeys);
    settings.setValue(QStringLiteral("removeTinyStitchesMax"), m_removeTinyStitchesMax);

    settings.endGroup();
    settings.endGroup();
}

void MainWindow::clearPaletteOverridesForCurrentImage() {
    m_dmcOverrides.clear();
    m_backgroundOverrideKeys.clear();
    m_removeTinyStitchesMax = 0;

    const QString imagePath = m_imageEdit ? m_imageEdit->text().trimmed() : QString();
    if (!imagePath.isEmpty()) {
        QSettings settings("JesteraceTools", "SpriteStitchCPP");
        settings.beginGroup(QStringLiteral("paletteOverrides"));
        settings.remove(paletteSettingsKey(imagePath));
        settings.endGroup();
    }
}

QString MainWindow::paletteOverrideSummary() const {
    QStringList parts;
    if (!m_dmcOverrides.isEmpty()) parts << QStringLiteral("%1 DMC override(s)").arg(m_dmcOverrides.size());
    if (!m_backgroundOverrideKeys.isEmpty()) parts << QStringLiteral("%1 background color override(s)").arg(m_backgroundOverrideKeys.size());
    if (m_removeTinyStitchesMax > 0) parts << QStringLiteral("remove colors with %1 stitch(es) or fewer").arg(m_removeTinyStitchesMax);
    return parts.isEmpty() ? QStringLiteral("No palette overrides") : parts.join(QStringLiteral(" | "));
}

void MainWindow::browseImage() {
    const QString path = QFileDialog::getOpenFileName(this, "Choose sprite image", lastUsefulFolder(),
                                                      "Images (*.png *.bmp *.gif *.jpg *.jpeg);;PNG files (*.png);;All files (*)");
    if (path.isEmpty()) return;

    const QString autoTitle = QFileInfo(path).completeBaseName().replace('_', ' ');
    m_imageEdit->setText(path);

    // When choosing a new sprite, the pattern title should follow the new sprite.
    // Project load still restores the saved project title through applyProjectJson().
    m_titleEdit->setText(autoTitle);

    if (m_outputEdit->text().trimmed().isEmpty()) {
        m_outputEdit->setText(QFileInfo(path).absolutePath());
    }
}

void MainWindow::chooseOutputFolder() {
    const QString path = QFileDialog::getExistingDirectory(this, "Choose output folder", lastUsefulFolder());
    if (!path.isEmpty()) m_outputEdit->setText(path);
}

void MainWindow::chooseBackgroundColor() {
    QColor chosen = QColorDialog::getColor(m_bgColor, this, "Choose background color to leave unstitched");
    if (chosen.isValid()) {
        m_bgColor = chosen;
        m_backgroundMode->setCurrentIndex(1);
        updateBackgroundSwatch();
        updateSpriteInfo();
        refreshPreviewScale();
    }
}

void MainWindow::updateBackgroundSwatch() {
    m_bgSwatch->setStyleSheet(QString("background-color: %1;").arg(m_bgColor.name()));
    const auto mode = static_cast<PatternOptions::BackgroundMode>(m_backgroundMode->currentData().toInt());
    const bool exact = mode == PatternOptions::BackgroundMode::ExactColor;
    m_bgColorButton->setEnabled(exact || m_backgroundMode->currentIndex() == 1);
}

PatternOptions MainWindow::collectOptions() const {
    PatternOptions options;
    options.title = m_titleEdit->text().trimmed();
    options.outputDir = m_outputEdit->text().trimmed();
    options.backgroundMode = static_cast<PatternOptions::BackgroundMode>(m_backgroundMode->currentData().toInt());
    options.backgroundColor = m_bgColor;
    options.transparencyAsBackground = m_transparencyUnstitchedCheck ? m_transparencyUnstitchedCheck->isChecked() : true;
    options.matchDmc = m_dmcCheck->isChecked();
    options.colorCleanupMode = static_cast<PatternOptions::ColorCleanupMode>(m_colorCleanupCombo->currentData().toInt());
    options.mergeTolerance = m_mergeToleranceSpin->value();
    options.maxColors = m_maxColorsSpin->value();
    options.dmcOverrides = m_dmcOverrides;
    options.backgroundColorKeys = m_backgroundOverrideKeys;
    options.removeTinyStitchesMax = m_removeTinyStitchesMax;
    const QString chartStyle = m_chartStyleCombo->currentData().toString();
    options.writeColorChart = (chartStyle == QStringLiteral("both") || chartStyle == QStringLiteral("color"));
    options.writeSymbolChart = (chartStyle == QStringLiteral("both") || chartStyle == QStringLiteral("symbols"));
    options.writeLegendCsv = m_csvCheck->isChecked();
    options.writePreviewPng = m_previewPngCheck ? m_previewPngCheck->isChecked() : true;
    options.symbolStyle = static_cast<PatternOptions::SymbolStyle>(m_symbolStyleCombo->currentData().toInt());
    options.openOutputFolder = m_openFolderCheck->isChecked();
    options.gridSize = static_cast<PatternOptions::GridSize>(m_gridSizeCombo->currentData().toInt());
    options.legendPlacement = static_cast<PatternOptions::LegendPlacement>(m_legendPlacementCombo->currentData().toInt());
    options.pageOrientation = static_cast<PatternOptions::PageOrientation>(m_pageOrientationCombo->currentData().toInt());
    options.drawCenterLines = m_centerLinesCheck->isChecked();
    options.includeCoverPage = m_coverPageCheck->isChecked();
    return options;
}


void MainWindow::useJesteraceWorkFolders() {
    const QString base = QDir::homePath() + QStringLiteral("/Projects/SpriteStitcherWork");
    const QString sprites = base + QStringLiteral("/sprites");
    const QString pdfs = base + QStringLiteral("/pdfs");
    const QString patterns = base + QStringLiteral("/patterns");

    QDir().mkpath(sprites);
    QDir().mkpath(pdfs);
    QDir().mkpath(patterns);

    if (m_outputEdit) {
        m_outputEdit->setText(pdfs);
    }

    if (m_imageEdit && m_imageEdit->text().trimmed().isEmpty()) {
        QDir spriteDir(sprites);
        const QStringList filters = QStringList()
                << QStringLiteral("*.png") << QStringLiteral("*.PNG")
                << QStringLiteral("*.bmp") << QStringLiteral("*.BMP")
                << QStringLiteral("*.gif") << QStringLiteral("*.GIF")
                << QStringLiteral("*.jpg") << QStringLiteral("*.JPG")
                << QStringLiteral("*.jpeg") << QStringLiteral("*.JPEG");
        const QFileInfoList files = spriteDir.entryInfoList(filters, QDir::Files, QDir::Name);
        if (!files.isEmpty()) {
            m_imageEdit->setText(files.first().absoluteFilePath());
        }
    }

    saveSettings();
    updateSpriteInfo();
    updatePreview();

    logLine(QStringLiteral("Using SpriteStitcher work folders: ") + base);
    logLine(QStringLiteral("Sprites folder: ") + sprites);
    logLine(QStringLiteral("PDF output folder: ") + pdfs);
    logLine(QStringLiteral("Project/pattern folder: ") + patterns);
}

void MainWindow::openSpritesFolder() {
    const QString folder = QDir::homePath() + QStringLiteral("/Projects/SpriteStitcherWork/sprites");
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void MainWindow::openPatternsFolder() {
    const QString folder = QDir::homePath() + QStringLiteral("/Projects/SpriteStitcherWork/patterns");
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}


void MainWindow::generate() {
    const QString imagePath = m_imageEdit->text().trimmed();
    if (imagePath.isEmpty()) {
        QMessageBox::warning(this, "Missing image", "Choose a sprite image first.");
        return;
    }

    const QFileInfo imageInfo(imagePath);
    if (!imageInfo.exists() || !imageInfo.isFile()) {
        QMessageBox::warning(this, "Image not found", "The selected sprite image does not exist anymore:\n" + imagePath);
        logLine("Generation stopped: selected image file was not found.");
        return;
    }
    if (!imageInfo.isReadable()) {
        QMessageBox::warning(this, "Image not readable", "The selected sprite image cannot be read:\n" + imagePath);
        logLine("Generation stopped: selected image file is not readable.");
        return;
    }

    PatternOptions options = collectOptions();
    if (!options.writeColorChart && !options.writeSymbolChart) {
        QMessageBox::warning(this, "No chart selected", "Choose at least one chart style before generating.");
        logLine("Generation stopped: no chart style selected.");
        return;
    }

    QDir outputDir(options.outputDir.isEmpty() ? imageInfo.absolutePath() : options.outputDir);
    if (QFileInfo(outputDir.absolutePath()).exists() && !QFileInfo(outputDir.absolutePath()).isDir()) {
        QMessageBox::warning(this, "Output path is not a folder", "The output path exists but is not a folder:\n" + outputDir.absolutePath());
        logLine("Generation stopped: output path is not a folder.");
        return;
    }
    if (!outputDir.exists()) {
        if (QMessageBox::question(this, "Create output folder",
                                  "The output folder does not exist. Create it now?\n" + outputDir.absolutePath(),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes) {
            logLine("Generation cancelled: output folder was not created.");
            return;
        }
        if (!outputDir.mkpath(QStringLiteral("."))) {
            QMessageBox::critical(this, "Output folder failed", "Could not create output folder:\n" + outputDir.absolutePath());
            logLine("Generation stopped: could not create output folder.");
            return;
        }
    }
    if (!QFileInfo(outputDir.absolutePath()).isWritable()) {
        QMessageBox::warning(this, "Output folder not writable", "The output folder is not writable:\n" + outputDir.absolutePath());
        logLine("Generation stopped: output folder is not writable.");
        return;
    }

    PatternResult info = m_engine.analyzeImage(imagePath, options);
    if (!info.ok) {
        QMessageBox::warning(this, "Image analysis failed", info.message.isEmpty()
                             ? QStringLiteral("Could not analyze the selected image.")
                             : info.message);
        logLine("Generation stopped: image analysis failed. " + info.message);
        return;
    }

    if (info.ok) {
        QStringList warnings;
        const int totalSquares = info.width * info.height;
        if (totalSquares >= 25000 || info.stitched >= 25000) {
            warnings << QString("Large pattern: %1 × %2 = %3 total squares, with %4 stitched squares.")
                        .arg(info.width).arg(info.height).arg(totalSquares).arg(info.stitched);
        }
        if (info.colorCount >= 50) {
            warnings << QString("High color count: %1 colors. This often happens with anti-aliased PNGs and can make the pattern harder to stitch.")
                        .arg(info.colorCount);
        }
        if (info.colorCountBeforeCleanup > 0 && info.colorCountBeforeCleanup != info.colorCount) {
            warnings << QString("Color cleanup is active: %1 colors before cleanup, %2 colors after cleanup.")
                        .arg(info.colorCountBeforeCleanup).arg(info.colorCount);
        }
        int fullyTransparent = 0;
        const int semiTransparent = countSemiTransparentPixels(imagePath, &fullyTransparent);
        if (semiTransparent > 0) {
            warnings << QString("Semi-transparent pixels found: %1. These can happen on anti-aliased edges and may affect the pattern. Consider using a clean pixel-art PNG if this was not intentional.")
                        .arg(semiTransparent);
        }

        if (!warnings.isEmpty()) {
            const QString warningText = warnings.join("\n\n") + "\n\nContinue generating this PDF?";
            if (QMessageBox::question(this, "Pattern size warning", warningText,
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                logLine("Generation cancelled after safety warning.");
                return;
            }
        }
    }

    m_generateButton->setEnabled(false);
    QApplication::setOverrideCursor(Qt::BusyCursor);
    logLine("Generating PDF...");

    PatternResult result = m_engine.generatePdf(imagePath, options);

    QApplication::restoreOverrideCursor();
    m_generateButton->setEnabled(true);

    if (!result.ok) {
        logLine("Error: " + result.message);
        QMessageBox::critical(this, "Generation failed", result.message);
        return;
    }

    m_lastGeneratedPdf = result.pdfPath;
    m_lastGeneratedFolder = QFileInfo(result.pdfPath).absolutePath();
    m_openPdfButton->setEnabled(QFileInfo::exists(m_lastGeneratedPdf));

    const int total = result.width * result.height;
    const QString pdfName = QFileInfo(result.pdfPath).fileName();
    const QString csvName = result.csvPath.isEmpty() ? QStringLiteral("Not written") : QFileInfo(result.csvPath).fileName();
    const QString pkName = result.patternKeeperPdfPath.isEmpty() ? QStringLiteral("Not written") : QFileInfo(result.patternKeeperPdfPath).fileName();
    const QString pngName = result.previewPngPath.isEmpty() ? QStringLiteral("Not written") : QFileInfo(result.previewPngPath).fileName();
    const QString chartStyleName = m_chartStyleCombo->currentText();
    const QString cleanupSummary = (result.colorCountBeforeCleanup > 0 && result.colorCountBeforeCleanup != result.colorCount)
            ? QString("%1 after cleanup/overrides (%2 before) | %3 | %4").arg(result.colorCount).arg(result.colorCountBeforeCleanup).arg(m_colorCleanupCombo->currentText()).arg(paletteOverrideSummary())
            : QString("%1 | %2 | %3").arg(result.colorCount).arg(m_colorCleanupCombo->currentText()).arg(paletteOverrideSummary());
    const QString layoutOptions = QString("Grid: %1 | Legend: %2 | Page: %3 | Center lines: %4 | Cover page: %5 | Symbols: %6")
            .arg(m_gridSizeCombo->currentText())
            .arg(m_legendPlacementCombo->currentText())
            .arg(m_pageOrientationCombo->currentText())
            .arg(m_centerLinesCheck->isChecked() ? QStringLiteral("On") : QStringLiteral("Off"))
            .arg(m_coverPageCheck->isChecked() ? QStringLiteral("Yes") : QStringLiteral("No"))
            .arg(m_symbolStyleCombo->currentText());
    const QString finishedSizes = QString("14ct: %1 × %2 in | 16ct: %3 × %4 in | 18ct: %5 × %6 in")
            .arg(result.width / 14.0, 0, 'f', 2).arg(result.height / 14.0, 0, 'f', 2)
            .arg(result.width / 16.0, 0, 'f', 2).arg(result.height / 16.0, 0, 'f', 2)
            .arg(result.width / 18.0, 0, 'f', 2).arg(result.height / 18.0, 0, 'f', 2);
    const QString summary = QString("Grid: %1 × %2 stitches\nTotal squares: %3\nStitched squares: %4\nUnstitched/background squares: %5\nColors: %6\nFinished size: %7\nChart style: %8\nPDF layout: %9\nPDF: %10\nPattern Keeper import PDF: %11\nCSV: %12\nWebsite preview PNG: %13")
            .arg(result.width)
            .arg(result.height)
            .arg(total)
            .arg(result.stitched)
            .arg(result.unstitched)
            .arg(cleanupSummary)
            .arg(finishedSizes)
            .arg(chartStyleName)
            .arg(layoutOptions)
            .arg(pdfName)
            .arg(pkName)
            .arg(csvName)
            .arg(pngName);
    m_summaryLabel->setText(summary);

    logLine(result.message);
    logLine("PDF: " + result.pdfPath);
    if (!result.patternKeeperPdfPath.isEmpty()) logLine("Pattern Keeper import PDF: " + result.patternKeeperPdfPath);
    if (!result.csvPath.isEmpty()) logLine("CSV: " + result.csvPath);
    if (!result.previewPngPath.isEmpty()) logLine("Website preview PNG: " + result.previewPngPath);

    saveSettings();

    if (m_openPdfCheck->isChecked()) openGeneratedPdf();
    if (m_openFolderCheck->isChecked()) openOutputFolder();
}

void MainWindow::openGeneratedPdf() {
    if (m_lastGeneratedPdf.isEmpty() || !QFileInfo::exists(m_lastGeneratedPdf)) {
        QMessageBox::information(this, "No generated PDF", "Generate a pattern first, then use this button to open the PDF.");
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastGeneratedPdf));
}

void MainWindow::openOutputFolder() {
    QString folder = m_lastGeneratedFolder;
    if (folder.isEmpty()) folder = m_outputEdit->text().trimmed();
    if (folder.isEmpty()) folder = lastUsefulFolder();
    if (!QDir(folder).exists()) {
        QMessageBox::warning(this, "Output folder missing", "That output folder does not exist anymore:\n" + folder);
        logLine("Open output folder failed: folder does not exist. " + folder);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void MainWindow::addBatchImagePath(const QString &path) {
    if (!m_batchTable) return;

    const QFileInfo info(path.trimmed());
    if (!info.exists() || !info.isFile()) return;

    const QString absolutePath = info.absoluteFilePath();
    const QString suffix = info.suffix().toLower();
    if (suffix != QStringLiteral("png") && suffix != QStringLiteral("bmp") && suffix != QStringLiteral("gif") &&
        suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg")) {
        return;
    }

    for (int row = 0; row < m_batchTable->rowCount(); ++row) {
        const auto *existing = m_batchTable->item(row, 0);
        if (existing && existing->data(Qt::UserRole).toString() == absolutePath) return;
    }

    const int row = m_batchTable->rowCount();
    m_batchTable->insertRow(row);

    auto *spriteItem = readOnlyItem(info.fileName());
    spriteItem->setData(Qt::UserRole, absolutePath);
    spriteItem->setToolTip(absolutePath);
    m_batchTable->setItem(row, 0, spriteItem);
    m_batchTable->setItem(row, 1, readOnlyItem(QStringLiteral("Waiting")));
    m_batchTable->setItem(row, 2, readOnlyItem(QString()));

    m_batchTable->resizeColumnsToContents();
    m_batchTable->horizontalHeader()->setStretchLastSection(true);

    if (m_imageEdit && m_imageEdit->text().trimmed().isEmpty()) {
        m_imageEdit->setText(absolutePath);
    }
    if (m_outputEdit && m_outputEdit->text().trimmed().isEmpty()) {
        m_outputEdit->setText(info.absolutePath());
    }

    if (m_batchStatusLabel) {
        m_batchStatusLabel->setText(QStringLiteral("%1 item(s) in batch queue.").arg(m_batchTable->rowCount()));
    }
}

QStringList MainWindow::batchImagePaths() const {
    QStringList paths;
    if (!m_batchTable) return paths;
    for (int row = 0; row < m_batchTable->rowCount(); ++row) {
        const auto *item = m_batchTable->item(row, 0);
        if (!item) continue;
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) paths << path;
    }
    return paths;
}

void MainWindow::setBatchRowStatus(int row, const QString &status, const QString &detail) {
    if (!m_batchTable || row < 0 || row >= m_batchTable->rowCount()) return;
    if (!m_batchTable->item(row, 1)) m_batchTable->setItem(row, 1, readOnlyItem(QString()));
    if (!m_batchTable->item(row, 2)) m_batchTable->setItem(row, 2, readOnlyItem(QString()));
    m_batchTable->item(row, 1)->setText(status);
    m_batchTable->item(row, 2)->setText(detail);
    m_batchTable->item(row, 2)->setToolTip(detail);
}

void MainWindow::addBatchFiles() {
    const QStringList paths = QFileDialog::getOpenFileNames(this, "Add sprites to batch queue", lastUsefulFolder(),
                                                            "Images (*.png *.bmp *.gif *.jpg *.jpeg);;PNG files (*.png);;All files (*)");
    if (paths.isEmpty()) return;

    const int before = m_batchTable ? m_batchTable->rowCount() : 0;
    for (const QString &path : paths) addBatchImagePath(path);
    const int added = (m_batchTable ? m_batchTable->rowCount() : 0) - before;
    logLine(QStringLiteral("Batch queue: added %1 file(s).").arg(added));
}

void MainWindow::addBatchFolder() {
    const QString folder = QFileDialog::getExistingDirectory(this, "Add sprite folder to batch queue", lastUsefulFolder());
    if (folder.isEmpty()) return;

    const int before = m_batchTable ? m_batchTable->rowCount() : 0;
    const QStringList filters = QStringList() << "*.png" << "*.PNG" << "*.bmp" << "*.BMP"
                                            << "*.gif" << "*.GIF" << "*.jpg" << "*.JPG"
                                            << "*.jpeg" << "*.JPEG";
    QDirIterator it(folder, filters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) addBatchImagePath(it.next());

    const int added = (m_batchTable ? m_batchTable->rowCount() : 0) - before;
    logLine(QStringLiteral("Batch queue: added %1 image(s) from folder %2.").arg(added).arg(folder));
    if (m_batchStatusLabel) {
        m_batchStatusLabel->setText(QStringLiteral("%1 item(s) in batch queue. Added %2 from the selected folder.")
                                    .arg(m_batchTable ? m_batchTable->rowCount() : 0).arg(added));
    }
}

void MainWindow::clearBatchQueue() {
    if (!m_batchTable || m_batchTable->rowCount() == 0) {
            return;
    }

    if (QMessageBox::question(this, "Clear batch queue", "Remove all sprites from the batch queue?",
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    m_batchTable->setRowCount(0);
    logLine(QStringLiteral("Batch queue cleared."));
}

void MainWindow::generateBatchQueue() {
    if (!m_batchTable || m_batchTable->rowCount() == 0) {
        QMessageBox::information(this, "Empty batch queue", "Add sprite files or a folder before using Generate all.");
        return;
    }

    PatternOptions batchBaseOptions = collectOptions();
    if (!batchBaseOptions.writeColorChart && !batchBaseOptions.writeSymbolChart) {
        QMessageBox::warning(this, "No chart selected", "Choose at least one chart style before generating the batch.");
        logLine("Batch stopped: no chart style selected.");
        return;
    }
    QString missingFiles;
    for (int row = 0; row < m_batchTable->rowCount(); ++row) {
        const QString path = m_batchTable->item(row, 0)->data(Qt::UserRole).toString();
        const QFileInfo fileInfo(path);
        if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
            missingFiles += fileInfo.fileName().isEmpty() ? path : fileInfo.fileName();
            missingFiles += QLatin1Char('\n');
            setBatchRowStatus(row, QStringLiteral("Failed"), QStringLiteral("Missing or unreadable file"));
        }
    }
    if (!missingFiles.isEmpty()) {
        QMessageBox::warning(this, "Batch has missing files",
                             "One or more batch files are missing or unreadable. Remove or re-add them before generating:\n\n" + missingFiles.trimmed());
        logLine("Batch stopped: missing or unreadable files found.");
        return;
    }

    QString outputBase = batchBaseOptions.outputDir;
    if (outputBase.isEmpty()) {
        const QString firstPath = m_batchTable->item(0, 0)->data(Qt::UserRole).toString();
        outputBase = QFileInfo(firstPath).absolutePath();
    }
    QDir batchOutputDir(outputBase);
    if (QFileInfo(batchOutputDir.absolutePath()).exists() && !QFileInfo(batchOutputDir.absolutePath()).isDir()) {
        QMessageBox::warning(this, "Output path is not a folder", "The output path exists but is not a folder:\n" + batchOutputDir.absolutePath());
        logLine("Batch stopped: output path is not a folder.");
        return;
    }
    if (!batchOutputDir.exists() && !batchOutputDir.mkpath(QStringLiteral("."))) {
        QMessageBox::critical(this, "Output folder failed", "Could not create output folder:\n" + batchOutputDir.absolutePath());
        logLine("Batch stopped: could not create output folder.");
        return;
    }
    if (!QFileInfo(batchOutputDir.absolutePath()).isWritable()) {
        QMessageBox::warning(this, "Output folder not writable", "The output folder is not writable:\n" + batchOutputDir.absolutePath());
        logLine("Batch stopped: output folder is not writable.");
        return;
    }

    const QString originalImage = m_imageEdit ? m_imageEdit->text().trimmed() : QString();

    QStringList warnings;
    int largeCount = 0;
    int highColorCount = 0;
    for (int row = 0; row < m_batchTable->rowCount(); ++row) {
        const QString path = m_batchTable->item(row, 0)->data(Qt::UserRole).toString();
        loadPaletteOverridesForImage(path);
        PatternOptions options = collectOptions();
        options.title = QFileInfo(path).completeBaseName().replace('_', ' ');
        const PatternResult info = m_engine.analyzeImage(path, options);
        if (!info.ok) continue;
        if ((info.width * info.height) >= 25000 || info.stitched >= 25000) ++largeCount;
        if (info.colorCount >= 50) ++highColorCount;
    }

    if (largeCount > 0) warnings << QStringLiteral("%1 large pattern(s) may take longer to generate.").arg(largeCount);
    if (highColorCount > 0) warnings << QStringLiteral("%1 pattern(s) still have 50+ colors after cleanup/overrides.").arg(highColorCount);
    if (!warnings.isEmpty()) {
        warnings << QStringLiteral("Continue generating the full batch?");
        if (QMessageBox::question(this, "Batch safety warning", warnings.join("\n\n"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            loadPaletteOverridesForImage(originalImage);
            updateSpriteInfo();
            logLine(QStringLiteral("Batch generation cancelled after safety warning."));
            return;
        }
    }

    loadPaletteOverridesForImage(originalImage);

    m_generateButton->setEnabled(false);
    m_generateBatchButton->setEnabled(false);
    m_addBatchFilesButton->setEnabled(false);
    m_addBatchFolderButton->setEnabled(false);
    m_clearBatchButton->setEnabled(false);
    QApplication::setOverrideCursor(Qt::BusyCursor);

    int done = 0;
    int failed = 0;
    const int total = m_batchTable->rowCount();
    if (m_batchProgress) {
        m_batchProgress->setRange(0, total);
        m_batchProgress->setValue(0);
    }

    logLine(QStringLiteral("Batch generation started: %1 item(s).").arg(total));

    for (int row = 0; row < total; ++row) {
        const QString path = m_batchTable->item(row, 0)->data(Qt::UserRole).toString();
        const QFileInfo fileInfo(path);
        setBatchRowStatus(row, QStringLiteral("Generating"), QString());
        if (m_batchStatusLabel) {
            m_batchStatusLabel->setText(QStringLiteral("Generating %1 of %2: %3")
                                        .arg(row + 1).arg(total).arg(fileInfo.fileName()));
        }
        QApplication::processEvents();

        loadPaletteOverridesForImage(path);
        PatternOptions options = collectOptions();
        options.title = fileInfo.completeBaseName().replace('_', ' ');

        const PatternResult result = m_engine.generatePdf(path, options);
        if (result.ok) {
            ++done;
            m_lastGeneratedPdf = result.pdfPath;
            m_lastGeneratedFolder = QFileInfo(result.pdfPath).absolutePath();
            const QString detail = result.previewPngPath.isEmpty()
                    ? QFileInfo(result.pdfPath).fileName()
                    : QStringLiteral("%1 + %2").arg(QFileInfo(result.pdfPath).fileName(), QFileInfo(result.previewPngPath).fileName());
            setBatchRowStatus(row, QStringLiteral("Done"), detail);
            logLine(QStringLiteral("Batch done: %1 -> %2").arg(fileInfo.fileName(), result.pdfPath));
            if (!result.previewPngPath.isEmpty()) logLine(QStringLiteral("Batch PNG: %1 -> %2").arg(fileInfo.fileName(), result.previewPngPath));
        } else {
            ++failed;
            setBatchRowStatus(row, QStringLiteral("Failed"), result.message);
            logLine(QStringLiteral("Batch failed: %1 -> %2").arg(fileInfo.fileName(), result.message));
        }

        if (m_batchProgress) m_batchProgress->setValue(row + 1);
        QApplication::processEvents();
    }

    loadPaletteOverridesForImage(originalImage);
    updateSpriteInfo();

    QApplication::restoreOverrideCursor();
    m_generateButton->setEnabled(true);
    m_generateBatchButton->setEnabled(true);
    m_addBatchFilesButton->setEnabled(true);
    m_addBatchFolderButton->setEnabled(true);
    m_clearBatchButton->setEnabled(true);
    if (m_openPdfButton) m_openPdfButton->setEnabled(!m_lastGeneratedPdf.isEmpty() && QFileInfo::exists(m_lastGeneratedPdf));

    const QString summary = QStringLiteral("Batch finished: %1 done, %2 failed, %3 total.").arg(done).arg(failed).arg(total);
    if (m_batchStatusLabel) m_batchStatusLabel->setText(summary);
    if (m_summaryLabel) m_summaryLabel->setText(summary + QStringLiteral("\nLast PDF: ") + (m_lastGeneratedPdf.isEmpty() ? QStringLiteral("None") : QFileInfo(m_lastGeneratedPdf).fileName()));
    logLine(summary);

    saveSettings();

    if (m_openFolderCheck->isChecked()) openOutputFolder();
}


void MainWindow::setComboByData(QComboBox *combo, const QVariant &value, int fallbackIndex) {
    if (!combo) return;
    const int idx = combo->findData(value);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    } else if (fallbackIndex >= 0 && fallbackIndex < combo->count()) {
        combo->setCurrentIndex(fallbackIndex);
    }
}

QJsonObject MainWindow::projectToJson() const {
    QJsonObject root;
    root[QStringLiteral("fileType")] = QStringLiteral("SpriteStitcherProject");
    root[QStringLiteral("formatVersion")] = 1;
    root[QStringLiteral("appVersion")] = QStringLiteral(APP_VERSION);
    root[QStringLiteral("spriteImage")] = m_imageEdit->text().trimmed();
    root[QStringLiteral("patternTitle")] = m_titleEdit->text().trimmed();
    root[QStringLiteral("outputFolder")] = m_outputEdit->text().trimmed();

    QJsonObject background;
    background[QStringLiteral("mode")] = m_backgroundMode->currentData().toInt();
    background[QStringLiteral("color")] = m_bgColor.name(QColor::HexArgb);
    background[QStringLiteral("transparencyAsBackground")] = m_transparencyUnstitchedCheck ? m_transparencyUnstitchedCheck->isChecked() : true;
    root[QStringLiteral("background")] = background;

    QJsonObject dmc;
    dmc[QStringLiteral("matchDmc")] = m_dmcCheck->isChecked();
    root[QStringLiteral("dmc")] = dmc;

    QJsonObject cleanup;
    cleanup[QStringLiteral("mode")] = m_colorCleanupCombo->currentData().toInt();
    cleanup[QStringLiteral("mergeTolerance")] = m_mergeToleranceSpin->value();
    cleanup[QStringLiteral("maxColors")] = m_maxColorsSpin->value();
    root[QStringLiteral("colorCleanup")] = cleanup;

    QJsonObject chart;
    chart[QStringLiteral("style")] = m_chartStyleCombo->currentData().toString();
    chart[QStringLiteral("symbolStyle")] = m_symbolStyleCombo->currentData().toInt();
    root[QStringLiteral("chart")] = chart;

    QJsonObject pdf;
    pdf[QStringLiteral("gridSize")] = m_gridSizeCombo->currentData().toInt();
    pdf[QStringLiteral("legendPlacement")] = m_legendPlacementCombo->currentData().toInt();
    pdf[QStringLiteral("pageOrientation")] = m_pageOrientationCombo->currentData().toInt();
    pdf[QStringLiteral("centerLines")] = m_centerLinesCheck->isChecked();
    pdf[QStringLiteral("coverPage")] = m_coverPageCheck->isChecked();
    root[QStringLiteral("pdfLayout")] = pdf;

    QJsonObject outputs;
    outputs[QStringLiteral("writeCsv")] = m_csvCheck->isChecked();
    outputs[QStringLiteral("writePreviewPng")] = m_previewPngCheck ? m_previewPngCheck->isChecked() : true;
    outputs[QStringLiteral("openPdfAfterGenerate")] = m_openPdfCheck->isChecked();
    outputs[QStringLiteral("openFolderAfterGenerate")] = m_openFolderCheck->isChecked();
    root[QStringLiteral("outputs")] = outputs;

    QJsonObject preview;
    preview[QStringLiteral("mode")] = m_previewModeCombo ? m_previewModeCombo->currentData().toString() : QStringLiteral("sprite");
    preview[QStringLiteral("scale")] = m_previewScaleCombo ? m_previewScaleCombo->currentData().toString() : QStringLiteral("fit");
    preview[QStringLiteral("checkerboard")] = m_previewCheckerboardCheck ? m_previewCheckerboardCheck->isChecked() : true;
    preview[QStringLiteral("pixelGrid")] = m_previewGridCheck ? m_previewGridCheck->isChecked() : false;
    root[QStringLiteral("preview")] = preview;


    QJsonObject palette;
    QJsonArray dmcOverrides;
    for (auto it = m_dmcOverrides.constBegin(); it != m_dmcOverrides.constEnd(); ++it) {
        QJsonObject entry;
        entry[QStringLiteral("key")] = it.key();
        entry[QStringLiteral("dmc")] = it.value();
        dmcOverrides.append(entry);
    }
    palette[QStringLiteral("dmcOverrides")] = dmcOverrides;

    QJsonArray backgroundKeys;
    QStringList sortedBackgroundKeys;
    for (const QString &key : m_backgroundOverrideKeys) sortedBackgroundKeys << key;
    sortedBackgroundKeys.sort(Qt::CaseInsensitive);
    for (const QString &key : sortedBackgroundKeys) backgroundKeys.append(key);
    palette[QStringLiteral("backgroundKeys")] = backgroundKeys;
    palette[QStringLiteral("removeTinyStitchesMax")] = m_removeTinyStitchesMax;
    root[QStringLiteral("paletteOverrides")] = palette;

    return root;
}

bool MainWindow::applyProjectJson(const QJsonObject &project, QString *errorMessage) {
    const QString fileType = project.value(QStringLiteral("fileType")).toString();
    if (fileType != QStringLiteral("SpriteStitchCPPProject") && fileType != QStringLiteral("SpriteStitcherProject")) {
        if (errorMessage) *errorMessage = QStringLiteral("This does not look like a SpriteStitch .sstitch project file.");
        return false;
    }

    m_imageEdit->setText(project.value(QStringLiteral("spriteImage")).toString());
    m_titleEdit->setText(project.value(QStringLiteral("patternTitle")).toString());
    m_outputEdit->setText(project.value(QStringLiteral("outputFolder")).toString());

    const QJsonObject background = project.value(QStringLiteral("background")).toObject();
    setComboByData(m_backgroundMode, background.value(QStringLiteral("mode")).toInt(0), 0);
    const QColor loadedBg(background.value(QStringLiteral("color")).toString(QStringLiteral("#ff00ff00")));
    m_bgColor = loadedBg.isValid() ? loadedBg : QColor(0, 255, 0);
    if (m_transparencyUnstitchedCheck) {
        m_transparencyUnstitchedCheck->setChecked(background.value(QStringLiteral("transparencyAsBackground")).toBool(true));
    }

    const QJsonObject dmc = project.value(QStringLiteral("dmc")).toObject();
    m_dmcCheck->setChecked(dmc.value(QStringLiteral("matchDmc")).toBool(true));

    const QJsonObject cleanup = project.value(QStringLiteral("colorCleanup")).toObject();
    setComboByData(m_colorCleanupCombo, cleanup.value(QStringLiteral("mode")).toInt(0), 0);
    m_mergeToleranceSpin->setValue(cleanup.value(QStringLiteral("mergeTolerance")).toInt(18));
    m_maxColorsSpin->setValue(cleanup.value(QStringLiteral("maxColors")).toInt(24));

    const QJsonObject chart = project.value(QStringLiteral("chart")).toObject();
    setComboByData(m_chartStyleCombo, chart.value(QStringLiteral("style")).toString(QStringLiteral("both")), 0);
    setComboByData(m_symbolStyleCombo, chart.value(QStringLiteral("symbolStyle")).toInt(static_cast<int>(PatternOptions::SymbolStyle::Clean)), 0);

    const QJsonObject pdf = project.value(QStringLiteral("pdfLayout")).toObject();
    setComboByData(m_gridSizeCombo, pdf.value(QStringLiteral("gridSize")).toInt(static_cast<int>(PatternOptions::GridSize::Medium)), 1);
    setComboByData(m_legendPlacementCombo, pdf.value(QStringLiteral("legendPlacement")).toInt(static_cast<int>(PatternOptions::LegendPlacement::SeparatePage)), 0);
    setComboByData(m_pageOrientationCombo, pdf.value(QStringLiteral("pageOrientation")).toInt(static_cast<int>(PatternOptions::PageOrientation::Auto)), 0);
    m_centerLinesCheck->setChecked(pdf.value(QStringLiteral("centerLines")).toBool(true));
    m_coverPageCheck->setChecked(pdf.value(QStringLiteral("coverPage")).toBool(false));

    const QJsonObject outputs = project.value(QStringLiteral("outputs")).toObject();
    m_csvCheck->setChecked(outputs.value(QStringLiteral("writeCsv")).toBool(true));
    if (m_previewPngCheck) m_previewPngCheck->setChecked(outputs.value(QStringLiteral("writePreviewPng")).toBool(true));
    m_openPdfCheck->setChecked(outputs.value(QStringLiteral("openPdfAfterGenerate")).toBool(false));
    m_openFolderCheck->setChecked(outputs.value(QStringLiteral("openFolderAfterGenerate")).toBool(false));

    const QJsonObject preview = project.value(QStringLiteral("preview")).toObject();
    if (m_previewModeCombo) setComboByData(m_previewModeCombo, preview.value(QStringLiteral("mode")).toString(QStringLiteral("sprite")), 0);
    if (m_previewScaleCombo) setComboByData(m_previewScaleCombo, preview.value(QStringLiteral("scale")).toString(QStringLiteral("fit")), 0);
    if (m_previewCheckerboardCheck) m_previewCheckerboardCheck->setChecked(preview.value(QStringLiteral("checkerboard")).toBool(true));
    if (m_previewGridCheck) m_previewGridCheck->setChecked(preview.value(QStringLiteral("pixelGrid")).toBool(false));

    m_dmcOverrides.clear();
    m_backgroundOverrideKeys.clear();
    m_removeTinyStitchesMax = 0;

    const QJsonObject palette = project.value(QStringLiteral("paletteOverrides")).toObject();
    const QJsonArray dmcOverrides = palette.value(QStringLiteral("dmcOverrides")).toArray();
    for (const QJsonValue &value : dmcOverrides) {
        const QJsonObject entry = value.toObject();
        const QString key = entry.value(QStringLiteral("key")).toString();
        const QString code = entry.value(QStringLiteral("dmc")).toString().trimmed();
        if (!key.isEmpty() && !code.isEmpty()) m_dmcOverrides.insert(key, code);
    }

    const QJsonArray backgroundKeys = palette.value(QStringLiteral("backgroundKeys")).toArray();
    for (const QJsonValue &value : backgroundKeys) {
        const QString key = value.toString();
        if (!key.isEmpty()) m_backgroundOverrideKeys.insert(key);
    }
    m_removeTinyStitchesMax = palette.value(QStringLiteral("removeTinyStitchesMax")).toInt(0);



    m_paletteLoadedForImage = m_imageEdit->text().trimmed();
    savePaletteOverridesForCurrentImage();
    updateBackgroundSwatch();
    updateColorCleanupControls();
    updatePreview();
    updateSpriteInfo();
    return true;
}

void MainWindow::saveProject() {
    const QString imagePath = m_imageEdit->text().trimmed();
    if (imagePath.isEmpty()) {
        QMessageBox::warning(this, "No sprite selected", "Choose a sprite image before saving a project file.");
        return;
    }

    savePaletteOverridesForCurrentImage();

    QString defaultPath = m_currentProjectPath;
    if (defaultPath.isEmpty()) {
        QString base = m_titleEdit->text().trimmed();
        if (base.isEmpty()) base = QFileInfo(imagePath).completeBaseName();
        base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
        if (base.isEmpty()) base = QStringLiteral("SpriteStitch_Project");
        defaultPath = QDir(lastUsefulFolder()).filePath(base + QStringLiteral(".sstitch"));
    }

    QString path = QFileDialog::getSaveFileName(this, "Save SpriteStitch project", defaultPath,
                                                "SpriteStitch projects (*.sstitch);;JSON files (*.json);;All files (*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".sstitch"), Qt::CaseInsensitive) && !path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".sstitch");
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Project save failed", "Could not write project file:\n" + path);
        return;
    }

    const QJsonDocument doc(projectToJson());
    const QByteArray json = doc.toJson(QJsonDocument::Indented);
    if (file.write(json) != static_cast<qint64>(json.size())) {
        QMessageBox::critical(this, "Project save failed", "Could not finish writing project file:\n" + path + "\n\n" + file.errorString());
        return;
    }
    file.close();

    m_currentProjectPath = path;
    saveSettings();
    logLine("Project saved: " + path);
    QMessageBox::information(this, "Project saved", "Saved project file:\n" + path);
}

void MainWindow::loadProject() {
    const QString startFolder = !m_currentProjectPath.isEmpty() ? QFileInfo(m_currentProjectPath).absolutePath() : lastUsefulFolder();
    const QString path = QFileDialog::getOpenFileName(this, "Load SpriteStitch project", startFolder,
                                                      "SpriteStitch projects (*.sstitch);;JSON files (*.json);;All files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Project load failed", "Could not read project file:\n" + path);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, "Project load failed",
                              "Could not parse project file:\n" + parseError.errorString());
        return;
    }

    QString error;
    if (!applyProjectJson(doc.object(), &error)) {
        QMessageBox::critical(this, "Project load failed", error);
        return;
    }

    m_currentProjectPath = path;
    saveSettings();
    logLine("Project loaded: " + path);
}

void MainWindow::reviewPalette() {
    const QString imagePath = m_imageEdit->text().trimmed();
    if (imagePath.isEmpty()) {
        QMessageBox::warning(this, "Missing image", "Choose a sprite image first, then review the palette.");
        return;
    }

    loadPaletteOverridesForImage(imagePath);

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Palette editor"));
    dialog.resize(980, 620);

    auto *layout = new QVBoxLayout(&dialog);

    auto *infoLabel = new QLabel(&dialog);
    infoLabel->setWordWrap(true);
    infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(infoLabel);

    auto *table = new QTableWidget(&dialog);
    table->setColumnCount(8);
    table->setHorizontalHeaderLabels({"Symbol", "Color", "DMC", "Description", "Stitches", "%", "RGB", "Source key"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    layout->addWidget(table, 1);

    PatternResult summary;
    QVector<PatternColor> colors;

    auto selectedRows = [table]() {
        QSet<int> rows;
        for (const auto *item : table->selectedItems()) rows.insert(item->row());
        QVector<int> sortedRows;
        sortedRows.reserve(rows.size());
        for (int row : rows) sortedRows.push_back(row);
        std::sort(sortedRows.begin(), sortedRows.end());
        return sortedRows;
    };

    auto keysForRow = [table](int row) {
        if (!table->item(row, 0)) return QStringList();
        return table->item(row, 0)->data(Qt::UserRole).toString().split(QChar(0x001F), Qt::SkipEmptyParts);
    };

    auto reloadPalette = [&]() -> bool {
        colors = m_engine.paletteForImage(imagePath, collectOptions(), &summary);
        if (!summary.ok) {
            infoLabel->setText(summary.message.isEmpty()
                               ? QStringLiteral("Could not analyze the selected sprite.")
                               : summary.message);
            table->setRowCount(0);
            return false;
        }

        const int totalSquares = summary.width * summary.height;
        int tinyColors = 0;
        for (const auto &color : colors) {
            if (color.stitches > 0 && color.stitches <= 3) ++tinyColors;
        }

        QStringList infoLines;
        infoLines << QStringLiteral("Grid: %1 × %2 | Total squares: %3 | Stitched: %4 | Unstitched/background: %5")
                         .arg(summary.width).arg(summary.height).arg(totalSquares).arg(summary.stitched).arg(summary.unstitched);
        if (summary.colorCountBeforeCleanup > 0 && summary.colorCountBeforeCleanup != summary.colorCount) {
            infoLines << QStringLiteral("Colors: %1 after cleanup/overrides (%2 before cleanup) | Cleanup mode: %3")
                             .arg(summary.colorCount).arg(summary.colorCountBeforeCleanup).arg(m_colorCleanupCombo->currentText());
        } else {
            infoLines << QStringLiteral("Colors: %1 | Cleanup mode: %2")
                             .arg(summary.colorCount).arg(m_colorCleanupCombo->currentText());
        }
        infoLines << QStringLiteral("Palette overrides: %1").arg(paletteOverrideSummary());
        if (tinyColors > 0) {
            infoLines << QStringLiteral("Tiny stray-color check: %1 color(s) use 3 stitches or fewer.").arg(tinyColors);
        }
        infoLines << QStringLiteral("Tip: select one or more rows, then use the buttons below to edit the final pattern palette.");
        infoLabel->setText(infoLines.join('\n'));
        dialog.setWindowTitle(QStringLiteral("Palette editor — %1 colors").arg(colors.size()));

        table->setSortingEnabled(false);
        table->clearContents();
        table->setRowCount(colors.size());

        for (int row = 0; row < colors.size(); ++row) {
            const PatternColor &color = colors[row];
            auto *symbol = readOnlyItem(color.symbol);
            symbol->setData(Qt::UserRole, color.key);
            table->setItem(row, 0, symbol);

            auto *swatch = readOnlyItem(QStringLiteral("     "));
            swatch->setBackground(color.color);
            swatch->setToolTip(color.color.name(QColor::HexRgb));
            table->setItem(row, 1, swatch);

            auto *dmc = new DmcTableItem(color.dmcCode);
            dmc->setFlags(dmc->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, 2, dmc);

            table->setItem(row, 3, readOnlyItem(color.dmcName));
            table->setItem(row, 4, readOnlyNumberItem(color.stitches));

            auto *percent = new QTableWidgetItem;
            const double pct = summary.stitched > 0 ? (100.0 * color.stitches / summary.stitched) : 0.0;
            percent->setData(Qt::DisplayRole, pct);
            percent->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            percent->setFlags(percent->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, 5, percent);

            table->setItem(row, 6, readOnlyItem(color.color.name(QColor::HexRgb).toUpper()));
            table->setItem(row, 7, readOnlyItem(color.key.split(QChar(0x001F), Qt::SkipEmptyParts).join(QStringLiteral(", "))));
        }

        table->resizeColumnsToContents();
        table->horizontalHeader()->setStretchLastSection(true);
        table->setSortingEnabled(true);
        table->sortItems(2, Qt::AscendingOrder);
        return true;
    };

    if (!reloadPalette()) {
        QMessageBox::warning(this, "Palette unavailable", summary.message.isEmpty()
                             ? QStringLiteral("Could not analyze the selected sprite.")
                             : summary.message);
        return;
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *changeDmcButton = buttonBox->addButton("Change selected DMC...", QDialogButtonBox::ActionRole);
    auto *backgroundButton = buttonBox->addButton("Mark selected as background", QDialogButtonBox::ActionRole);
    auto *removeTinyButton = buttonBox->addButton("Remove tiny colors...", QDialogButtonBox::ActionRole);
    auto *clearOverridesButton = buttonBox->addButton("Clear palette overrides", QDialogButtonBox::ActionRole);
    auto *sortDmcButton = buttonBox->addButton("Sort by DMC", QDialogButtonBox::ActionRole);
    auto *sortStitchesButton = buttonBox->addButton("Sort by stitch count", QDialogButtonBox::ActionRole);

    connect(changeDmcButton, &QPushButton::clicked, &dialog, [&] {
        const QVector<int> rows = selectedRows();
        if (rows.isEmpty()) {
            QMessageBox::information(&dialog, "No color selected", "Select one or more palette rows first.");
            return;
        }

        QVector<DmcColor> dmcList = m_engine.dmcPalette();
        std::sort(dmcList.begin(), dmcList.end(), [](const DmcColor &a, const DmcColor &b) {
            return dmcSortKey(a.code) < dmcSortKey(b.code);
        });

        QDialog dmcDialog(&dialog);
        dmcDialog.setWindowTitle(QStringLiteral("Choose DMC color"));
        auto *dmcLayout = new QVBoxLayout(&dmcDialog);
        auto *label = new QLabel(QStringLiteral("Choose the DMC floss to use for the selected color(s):"), &dmcDialog);
        label->setWordWrap(true);
        dmcLayout->addWidget(label);
        auto *combo = new QComboBox(&dmcDialog);
        for (const auto &dmc : dmcList) {
            combo->addItem(QStringLiteral("%1 — %2").arg(dmc.code, dmc.name), dmc.code);
        }
        if (!rows.isEmpty() && table->item(rows.first(), 2)) {
            const QString currentCode = table->item(rows.first(), 2)->text();
            const int idx = combo->findData(currentCode, Qt::UserRole, Qt::MatchFixedString);
            if (idx >= 0) combo->setCurrentIndex(idx);
        }
        dmcLayout->addWidget(combo);
        auto *dmcButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dmcDialog);
        connect(dmcButtons, &QDialogButtonBox::accepted, &dmcDialog, &QDialog::accept);
        connect(dmcButtons, &QDialogButtonBox::rejected, &dmcDialog, &QDialog::reject);
        dmcLayout->addWidget(dmcButtons);

        if (dmcDialog.exec() != QDialog::Accepted) return;
        const QString code = combo->currentData().toString();
        for (int row : rows) {
            for (const QString &key : keysForRow(row)) {
                m_dmcOverrides.insert(key, code);
                m_backgroundOverrideKeys.remove(key);
            }
        }
        savePaletteOverridesForCurrentImage();
        reloadPalette();
        updateSpriteInfo();
        refreshPreviewScale();
        logLine(QStringLiteral("Palette override: changed %1 selected row(s) to DMC %2.").arg(rows.size()).arg(code));
    });

    connect(backgroundButton, &QPushButton::clicked, &dialog, [&] {
        const QVector<int> rows = selectedRows();
        if (rows.isEmpty()) {
            QMessageBox::information(&dialog, "No color selected", "Select one or more palette rows first.");
            return;
        }
        for (int row : rows) {
            for (const QString &key : keysForRow(row)) {
                m_backgroundOverrideKeys.insert(key);
                m_dmcOverrides.remove(key);
            }
        }
        savePaletteOverridesForCurrentImage();
        reloadPalette();
        updateSpriteInfo();
        refreshPreviewScale();
        logLine(QStringLiteral("Palette override: marked %1 selected row(s) as unstitched background.").arg(rows.size()));
    });

    connect(removeTinyButton, &QPushButton::clicked, &dialog, [&] {
        bool ok = false;
        const int value = QInputDialog::getInt(&dialog, "Remove tiny colors",
                                               "Remove colors with this many stitches or fewer:",
                                               m_removeTinyStitchesMax > 0 ? m_removeTinyStitchesMax : 3,
                                               1, 50, 1, &ok);
        if (!ok) return;
        m_removeTinyStitchesMax = value;
        savePaletteOverridesForCurrentImage();
        reloadPalette();
        updateSpriteInfo();
        refreshPreviewScale();
        logLine(QStringLiteral("Palette override: tiny colors with %1 stitch(es) or fewer will be left unstitched.").arg(value));
    });

    connect(clearOverridesButton, &QPushButton::clicked, &dialog, [&] {
        if (m_dmcOverrides.isEmpty() && m_backgroundOverrideKeys.isEmpty() && m_removeTinyStitchesMax <= 0) {
            QMessageBox::information(&dialog, "No overrides", "There are no palette overrides to clear for this sprite.");
            return;
        }
        if (QMessageBox::question(&dialog, "Clear palette overrides",
                                  "Clear all DMC changes, background markings, and tiny-color removal for this sprite?",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
        clearPaletteOverridesForCurrentImage();
        reloadPalette();
        updateSpriteInfo();
        refreshPreviewScale();
        logLine(QStringLiteral("Palette overrides cleared for current sprite."));
    });

    connect(sortDmcButton, &QPushButton::clicked, table, [table] { table->sortItems(2, Qt::AscendingOrder); });
    connect(sortStitchesButton, &QPushButton::clicked, table, [table] { table->sortItems(4, Qt::DescendingOrder); });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    logLine(QStringLiteral("Palette editor opened: %1 colors, %2 stitched squares. %3")
            .arg(colors.size()).arg(summary.stitched).arg(paletteOverrideSummary()));
    dialog.exec();
}

void MainWindow::resetSettings() {
    if (QMessageBox::question(this, "Reset saved settings",
                              "Reset saved app settings, window layout, and palette override cache?\n\nThis does not delete generated PDFs or project files.",
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        logLine("Reset saved settings cancelled.");
        return;
    }

    QSettings settings("JesteraceTools", "SpriteStitchCPP");
    settings.clear();
    m_imageEdit->clear();
    m_titleEdit->clear();
    m_outputEdit->clear();
    m_backgroundMode->setCurrentIndex(0);
    m_bgColor = QColor(0, 255, 0);
    if (m_transparencyUnstitchedCheck) m_transparencyUnstitchedCheck->setChecked(true);
    m_dmcCheck->setChecked(true);
    if (m_colorCleanupCombo) m_colorCleanupCombo->setCurrentIndex(0);
    if (m_mergeToleranceSpin) m_mergeToleranceSpin->setValue(18);
    if (m_maxColorsSpin) m_maxColorsSpin->setValue(24);
    if (m_chartStyleCombo) m_chartStyleCombo->setCurrentIndex(0);
    if (m_gridSizeCombo) m_gridSizeCombo->setCurrentIndex(1);
    if (m_legendPlacementCombo) m_legendPlacementCombo->setCurrentIndex(0);
    if (m_pageOrientationCombo) m_pageOrientationCombo->setCurrentIndex(0);
    if (m_previewModeCombo) m_previewModeCombo->setCurrentIndex(0);
    if (m_previewScaleCombo) m_previewScaleCombo->setCurrentIndex(0);
    if (m_previewCheckerboardCheck) m_previewCheckerboardCheck->setChecked(true);
    if (m_previewGridCheck) m_previewGridCheck->setChecked(false);
    if (m_centerLinesCheck) m_centerLinesCheck->setChecked(true);
    if (m_coverPageCheck) m_coverPageCheck->setChecked(false);
    m_csvCheck->setChecked(true);
    m_openFolderCheck->setChecked(false);
    m_openPdfCheck->setChecked(false);
    m_lastGeneratedPdf.clear();
    m_lastGeneratedFolder.clear();
    m_currentProjectPath.clear();
    m_paletteLoadedForImage.clear();
    m_dmcOverrides.clear();
    m_backgroundOverrideKeys.clear();
    m_removeTinyStitchesMax = 0;
    if (m_openPdfButton) m_openPdfButton->setEnabled(false);
    if (m_summaryLabel) m_summaryLabel->setText("No pattern generated yet.");
    updateBackgroundSwatch();
    updateColorCleanupControls();
    updatePreview();
    logLine("Saved settings reset.");
}

void MainWindow::loadSettings() {
    QSettings settings("JesteraceTools", "SpriteStitchCPP");
    restoreGeometry(settings.value("window/geometry").toByteArray());
    if (m_mainSplitter) {
        const QByteArray splitterState = settings.value("window/mainSplitter").toByteArray();
        if (!splitterState.isEmpty()) m_mainSplitter->restoreState(splitterState);
    }
    if (m_leftSplitter) {
        const QByteArray leftSplitterState = settings.value("window/leftSplitter").toByteArray();
        if (!leftSplitterState.isEmpty()) m_leftSplitter->restoreState(leftSplitterState);
    }
    if (m_rightSplitter) {
        const QByteArray rightSplitterState = settings.value("window/rightSplitter").toByteArray();
        if (!rightSplitterState.isEmpty()) m_rightSplitter->restoreState(rightSplitterState);
    }
    m_imageEdit->setText(settings.value("paths/image").toString());
    m_titleEdit->setText(settings.value("pattern/title").toString());
    m_outputEdit->setText(settings.value("paths/outputDir").toString());
    m_backgroundMode->setCurrentIndex(settings.value("pattern/backgroundMode", 0).toInt());
    m_bgColor = settings.value("pattern/backgroundColor", QColor(0, 255, 0)).value<QColor>();
    if (m_transparencyUnstitchedCheck) m_transparencyUnstitchedCheck->setChecked(settings.value("pattern/transparencyAsBackground", true).toBool());
    m_dmcCheck->setChecked(settings.value("pattern/matchDmc", true).toBool());
    auto restoreComboByInt = [](QComboBox *combo, int value, int fallbackIndex) {
        if (!combo) return;
        const int idx = combo->findData(value);
        combo->setCurrentIndex(idx >= 0 ? idx : fallbackIndex);
    };
    restoreComboByInt(m_colorCleanupCombo, settings.value("color/cleanupMode", static_cast<int>(PatternOptions::ColorCleanupMode::None)).toInt(), 0);
    m_mergeToleranceSpin->setValue(settings.value("color/mergeTolerance", 18).toInt());
    m_maxColorsSpin->setValue(settings.value("color/maxColors", 24).toInt());
    const QString savedChartStyle = settings.value("outputs/chartStyle").toString();
    if (!savedChartStyle.isEmpty()) {
        const int idx = m_chartStyleCombo->findData(savedChartStyle);
        if (idx >= 0) m_chartStyleCombo->setCurrentIndex(idx);
    } else {
        // Migrate older v1.6 checkbox settings if present.
        const bool oldColor = settings.value("outputs/colorChart", true).toBool();
        const bool oldSymbols = settings.value("outputs/symbolChart", true).toBool();
        const QString migrated = oldColor && oldSymbols ? QStringLiteral("both")
                                : oldColor ? QStringLiteral("color")
                                : oldSymbols ? QStringLiteral("symbols")
                                : QStringLiteral("both");
        const int idx = m_chartStyleCombo->findData(migrated);
        if (idx >= 0) m_chartStyleCombo->setCurrentIndex(idx);
    }
    restoreComboByInt(m_symbolStyleCombo, settings.value("outputs/symbolStyle", static_cast<int>(PatternOptions::SymbolStyle::Clean)).toInt(), 0);
    restoreComboByInt(m_gridSizeCombo, settings.value("pdf/gridSize", static_cast<int>(PatternOptions::GridSize::Medium)).toInt(), 1);
    restoreComboByInt(m_legendPlacementCombo, settings.value("pdf/legendPlacement", static_cast<int>(PatternOptions::LegendPlacement::SeparatePage)).toInt(), 0);
    restoreComboByInt(m_pageOrientationCombo, settings.value("pdf/pageOrientation", static_cast<int>(PatternOptions::PageOrientation::Auto)).toInt(), 0);
    m_centerLinesCheck->setChecked(settings.value("pdf/centerLines", true).toBool());
    m_coverPageCheck->setChecked(settings.value("pdf/coverPage", false).toBool());
    m_csvCheck->setChecked(settings.value("outputs/csv", true).toBool());
    if (m_previewPngCheck) m_previewPngCheck->setChecked(settings.value("outputs/previewPng", true).toBool());
    m_openFolderCheck->setChecked(settings.value("outputs/openFolder", false).toBool());
    m_openPdfCheck->setChecked(settings.value("outputs/openPdf", false).toBool());
    if (m_previewModeCombo) {
        const QString savedPreviewMode = settings.value("preview/mode", QStringLiteral("sprite")).toString();
        const int idx = m_previewModeCombo->findData(savedPreviewMode);
        if (idx >= 0) m_previewModeCombo->setCurrentIndex(idx);
    }
    if (m_previewScaleCombo) {
        const QString savedPreviewScale = settings.value("preview/scale", QStringLiteral("fit")).toString();
        const int idx = m_previewScaleCombo->findData(savedPreviewScale);
        if (idx >= 0) m_previewScaleCombo->setCurrentIndex(idx);
    }
    if (m_previewCheckerboardCheck) m_previewCheckerboardCheck->setChecked(settings.value("preview/checkerboard", true).toBool());
    if (m_previewGridCheck) m_previewGridCheck->setChecked(settings.value("preview/pixelGrid", false).toBool());
    m_lastGeneratedFolder = settings.value("paths/lastGeneratedFolder").toString();
    m_lastGeneratedPdf = settings.value("paths/lastGeneratedPdf").toString();
    m_currentProjectPath = settings.value("paths/currentProject").toString();
    if (m_openPdfButton) m_openPdfButton->setEnabled(!m_lastGeneratedPdf.isEmpty() && QFileInfo::exists(m_lastGeneratedPdf));
    updateBackgroundSwatch();
    updateColorCleanupControls();
    updatePreview();
}

void MainWindow::saveSettings() {
    QSettings settings("JesteraceTools", "SpriteStitchCPP");
    settings.setValue("window/geometry", saveGeometry());
    if (m_mainSplitter) settings.setValue("window/mainSplitter", m_mainSplitter->saveState());
    if (m_leftSplitter) settings.setValue("window/leftSplitter", m_leftSplitter->saveState());
    if (m_rightSplitter) settings.setValue("window/rightSplitter", m_rightSplitter->saveState());
    settings.setValue("paths/image", m_imageEdit->text().trimmed());
    settings.setValue("pattern/title", m_titleEdit->text().trimmed());
    settings.setValue("paths/outputDir", m_outputEdit->text().trimmed());
    settings.setValue("pattern/backgroundMode", m_backgroundMode->currentIndex());
    settings.setValue("pattern/backgroundColor", m_bgColor);
    settings.setValue("pattern/transparencyAsBackground", m_transparencyUnstitchedCheck ? m_transparencyUnstitchedCheck->isChecked() : true);
    settings.setValue("pattern/matchDmc", m_dmcCheck->isChecked());
    settings.setValue("color/cleanupMode", m_colorCleanupCombo->currentData().toInt());
    settings.setValue("color/mergeTolerance", m_mergeToleranceSpin->value());
    settings.setValue("color/maxColors", m_maxColorsSpin->value());
    settings.setValue("outputs/chartStyle", m_chartStyleCombo->currentData().toString());
    settings.setValue("outputs/symbolStyle", m_symbolStyleCombo->currentData().toInt());
    settings.setValue("pdf/gridSize", m_gridSizeCombo->currentData().toInt());
    settings.setValue("pdf/legendPlacement", m_legendPlacementCombo->currentData().toInt());
    settings.setValue("pdf/pageOrientation", m_pageOrientationCombo->currentData().toInt());
    settings.setValue("pdf/centerLines", m_centerLinesCheck->isChecked());
    settings.setValue("pdf/coverPage", m_coverPageCheck->isChecked());
    settings.setValue("outputs/csv", m_csvCheck->isChecked());
    settings.setValue("outputs/previewPng", m_previewPngCheck ? m_previewPngCheck->isChecked() : true);
    settings.setValue("outputs/openFolder", m_openFolderCheck->isChecked());
    settings.setValue("outputs/openPdf", m_openPdfCheck->isChecked());
    settings.setValue("preview/mode", m_previewModeCombo ? m_previewModeCombo->currentData().toString() : QStringLiteral("sprite"));
    settings.setValue("preview/scale", m_previewScaleCombo ? m_previewScaleCombo->currentData().toString() : QStringLiteral("fit"));
    settings.setValue("preview/checkerboard", m_previewCheckerboardCheck ? m_previewCheckerboardCheck->isChecked() : true);
    settings.setValue("preview/pixelGrid", m_previewGridCheck ? m_previewGridCheck->isChecked() : false);
    settings.setValue("paths/lastGeneratedFolder", m_lastGeneratedFolder);
    settings.setValue("paths/lastGeneratedPdf", m_lastGeneratedPdf);
    settings.setValue("paths/currentProject", m_currentProjectPath);
    savePaletteOverridesForCurrentImage();
}

void MainWindow::logLine(const QString &text) {
    m_log->append(text);
}
