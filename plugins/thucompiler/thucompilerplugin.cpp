/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <interfaces/icore.h>
#include <interfaces/iplugin.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iuicontroller.h>
#include <util/path.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

class ThuCompilerPlugin;

enum class CompileMode
{
    Fft,
    Ifft,
    Thc,
};

class ModeCard : public QFrame
{
    Q_OBJECT

public:
    ModeCard(const QString& title, const QString& glyph, const QString& line1, const QString& line2, QWidget* parent = nullptr)
        : QFrame(parent)
        , m_radio(new QRadioButton(this))
    {
        setObjectName(QStringLiteral("modeCard"));
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::PointingHandCursor);

        auto* titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("modeTitle"));
        auto* glyphLabel = new QLabel(glyph, this);
        glyphLabel->setObjectName(QStringLiteral("modeGlyph"));
        glyphLabel->setAlignment(Qt::AlignCenter);
        auto* detailLabel = new QLabel(line1 + QLatin1Char('\n') + line2, this);
        detailLabel->setAlignment(Qt::AlignCenter);

        auto* titleRow = new QHBoxLayout;
        titleRow->addWidget(m_radio);
        titleRow->addWidget(titleLabel);
        titleRow->addStretch();

        auto* layout = new QVBoxLayout(this);
        layout->addLayout(titleRow);
        layout->addWidget(glyphLabel, 1);
        layout->addWidget(detailLabel);
    }

    QRadioButton* radio() const
    {
        return m_radio;
    }

Q_SIGNALS:
    void selected();

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        m_radio->setChecked(true);
        Q_EMIT selected();
        QFrame::mousePressEvent(event);
    }

private:
    QRadioButton* const m_radio;
};

class ThuCompilerView : public QWidget
{
    Q_OBJECT

public:
    explicit ThuCompilerView(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_modeGroup(new QButtonGroup(this))
        , m_fileList(new QListWidget(this))
        , m_editor(new QPlainTextEdit(this))
        , m_output(new QPlainTextEdit(this))
        , m_statusLabel(new QLabel(this))
        , m_optionStack(new QStackedWidget(this))
        , m_optCombo(new QComboBox(this))
        , m_fftPotCombo(new QComboBox(this))
        , m_ifftPotCombo(new QComboBox(this))
        , m_outputDirEdit(new QPlainTextEdit(this))
        , m_compilerRootLabel(new QLabel(this))
        , m_compileButton(new QPushButton(QStringLiteral("开始编译"), this))
        , m_saveButton(new QPushButton(QStringLiteral("保存"), this))
        , m_formatButton(new QPushButton(QStringLiteral("格式化"), this))
        , m_addFileButton(new QPushButton(QStringLiteral("添加文件"), this))
        , m_removeFileButton(new QPushButton(QStringLiteral("移除"), this))
        , m_clearOutputButton(new QPushButton(QStringLiteral("清空"), this))
        , m_copyOutputButton(new QPushButton(QStringLiteral("复制"), this))
        , m_openOutputDirButton(new QPushButton(QStringLiteral("打开输出目录"), this))
        , m_fullscreenButton(new QPushButton(QStringLiteral("全屏"), this))
        , m_changeCompilerButton(new QPushButton(QStringLiteral("选择..."), this))
        , m_chooseOutputDirButton(new QPushButton(QStringLiteral("选择..."), this))
        , m_process(new QProcess(this))
    {
        buildUi();
        readSettings();
        updateCompilerRootLabel();
        updateModeUi();

        connect(m_fileList, &QListWidget::currentRowChanged, this, &ThuCompilerView::loadSelectedFile);
        connect(m_addFileButton, &QPushButton::clicked, this, &ThuCompilerView::addFiles);
        connect(m_removeFileButton, &QPushButton::clicked, this, &ThuCompilerView::removeSelectedFile);
        connect(m_saveButton, &QPushButton::clicked, this, &ThuCompilerView::saveCurrentFile);
        connect(m_formatButton, &QPushButton::clicked, this, &ThuCompilerView::formatCurrentFile);
        connect(m_compileButton, &QPushButton::clicked, this, &ThuCompilerView::compile);
        connect(m_clearOutputButton, &QPushButton::clicked, m_output, &QPlainTextEdit::clear);
        connect(m_copyOutputButton, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setText(m_output->toPlainText());
        });
        connect(m_openOutputDirButton, &QPushButton::clicked, this, &ThuCompilerView::openOutputDirectory);
        connect(m_fullscreenButton, &QPushButton::clicked, this, &ThuCompilerView::toggleFullScreen);
        connect(m_changeCompilerButton, &QPushButton::clicked, this, &ThuCompilerView::chooseCompilerRoot);
        connect(m_chooseOutputDirButton, &QPushButton::clicked, this, &ThuCompilerView::chooseOutputDirectory);
        connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
            m_mode = static_cast<CompileMode>(id);
            updateModeUi();
        });
        connect(m_process, &QProcess::readyReadStandardOutput, this, &ThuCompilerView::readProcessOutput);
        connect(m_process, &QProcess::readyReadStandardError, this, &ThuCompilerView::readProcessOutput);
        connect(m_process, &QProcess::finished, this, &ThuCompilerView::processFinished);
        connect(m_process, &QProcess::errorOccurred, this, &ThuCompilerView::processError);
    }

    ~ThuCompilerView() override
    {
        writeSettings();
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange) {
            scheduleThemeStyleSheetUpdate();
        }
    }

private:
    void buildUi()
    {
        setObjectName(QStringLiteral("thuCompilerView"));
        applyThemeStyleSheet();

        auto* fftCard = new ModeCard(QStringLiteral("FFT 模式 (-FFT)"), QStringLiteral("~"), QStringLiteral("编译 FFT 相关电路"), QStringLiteral("输入 pot_num"), this);
        auto* ifftCard = new ModeCard(QStringLiteral("IFFT 模式 (-IFFT)"), QStringLiteral("≈"), QStringLiteral("编译 IFFT 相关电路"), QStringLiteral("输入 pot_num"), this);
        auto* thcCard = new ModeCard(QStringLiteral("THC 模式 (-THC)"), QStringLiteral("</>"), QStringLiteral("编译 THC 程序"), QStringLiteral("需要依赖 .c 文件"), this);

        m_modeGroup->addButton(fftCard->radio(), static_cast<int>(CompileMode::Fft));
        m_modeGroup->addButton(ifftCard->radio(), static_cast<int>(CompileMode::Ifft));
        m_modeGroup->addButton(thcCard->radio(), static_cast<int>(CompileMode::Thc));
        thcCard->radio()->setChecked(true);
        connect(fftCard, &ModeCard::selected, this, [this]() {
            m_mode = CompileMode::Fft;
            updateModeUi();
        });
        connect(ifftCard, &ModeCard::selected, this, [this]() {
            m_mode = CompileMode::Ifft;
            updateModeUi();
        });
        connect(thcCard, &ModeCard::selected, this, [this]() {
            m_mode = CompileMode::Thc;
            updateModeUi();
        });

        auto* modeLayout = new QHBoxLayout;
        modeLayout->addWidget(fftCard);
        modeLayout->addWidget(ifftCard);
        modeLayout->addWidget(thcCard);

        auto* fileButtons = new QHBoxLayout;
        fileButtons->addWidget(m_addFileButton);
        fileButtons->addWidget(m_removeFileButton);

        auto* filePanel = new QVBoxLayout;
        filePanel->addLayout(fileButtons);
        filePanel->addWidget(m_fileList, 1);

        auto* editorActions = new QHBoxLayout;
        editorActions->addStretch();
        editorActions->addWidget(m_saveButton);
        editorActions->addWidget(m_formatButton);

        m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_editor->setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);

        auto* editorPanel = new QVBoxLayout;
        editorPanel->addLayout(editorActions);
        editorPanel->addWidget(m_editor, 1);

        auto* sourceLayout = new QHBoxLayout;
        sourceLayout->addLayout(filePanel, 1);
        sourceLayout->addLayout(editorPanel, 4);

        auto* sourceGroup = new QGroupBox(QStringLiteral("源码编辑"), this);
        sourceGroup->setLayout(sourceLayout);

        auto* outputActions = new QHBoxLayout;
        outputActions->addWidget(new QLabel(QStringLiteral("编译输出"), this));
        outputActions->addStretch();
        outputActions->addWidget(m_clearOutputButton);
        outputActions->addWidget(m_copyOutputButton);
        outputActions->addWidget(m_openOutputDirButton);

        m_output->setReadOnly(true);
        m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_output->setMaximumBlockCount(5000);

        auto* titleRow = new QHBoxLayout;
        titleRow->addWidget(new QLabel(QStringLiteral("编译配置"), this));
        titleRow->addStretch();
        m_compileButton->setObjectName(QStringLiteral("primaryButton"));
        titleRow->addWidget(m_compileButton);
        titleRow->addWidget(m_fullscreenButton);

        auto* mainColumn = new QVBoxLayout;
        mainColumn->addLayout(titleRow);
        mainColumn->addLayout(modeLayout);
        mainColumn->addWidget(sourceGroup, 3);
        mainColumn->addLayout(outputActions);
        mainColumn->addWidget(m_statusLabel);
        mainColumn->addWidget(m_output, 1);

        auto* sidePanel = new QFrame(this);
        sidePanel->setObjectName(QStringLiteral("sidePanel"));
        auto* sideLayout = new QVBoxLayout(sidePanel);
        sideLayout->addWidget(new QLabel(QStringLiteral("编译选项"), this));
        sideLayout->addWidget(m_optionStack);
        sideLayout->addSpacing(8);
        sideLayout->addWidget(new QLabel(QStringLiteral("输出目录"), this));
        m_outputDirEdit->setFixedHeight(50);
        m_outputDirEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
        sideLayout->addWidget(m_outputDirEdit);
        sideLayout->addWidget(m_chooseOutputDirButton);
        sideLayout->addSpacing(8);
        sideLayout->addWidget(new QLabel(QStringLiteral("编译器目录"), this));
        m_compilerRootLabel->setWordWrap(true);
        sideLayout->addWidget(m_compilerRootLabel);
        sideLayout->addWidget(m_changeCompilerButton);
        sideLayout->addStretch();

        setupOptionPages();

        m_contentWidget = new QWidget(this);
        auto* content = new QHBoxLayout(m_contentWidget);
        content->setContentsMargins(0, 0, 0, 0);
        content->addLayout(mainColumn, 5);
        content->addWidget(sidePanel, 1);

        m_rootLayout = new QHBoxLayout(this);
        m_rootLayout->setContentsMargins(12, 12, 12, 12);
        m_rootLayout->addWidget(m_contentWidget, 1);
    }

    void applyThemeStyleSheet()
    {
        if (m_applyingThemeStyleSheet) {
            return;
        }
        m_applyingThemeStyleSheet = true;

        const QPalette palette = QApplication::palette();
        const bool dark = palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();
        if (dark) {
            setStyleSheet(QStringLiteral(
                "#thuCompilerView { background: #1e1e1e; color: #d4d4d4; }"
                "#thuCompilerView QLabel { color: #d4d4d4; }"
                "#modeCard { border: 1px solid #3c3c3c; border-radius: 8px; background: #252526; color: #d4d4d4; }"
                "#modeCard:hover { border-color: #007acc; background: #2a2d2e; }"
                "#modeTitle { font-weight: 600; }"
                "#modeGlyph { color: #4fc1ff; font-size: 42px; font-weight: 600; }"
                "#sidePanel { border-left: 1px solid #3c3c3c; background: #252526; color: #cccccc; }"
                "#outputTitle { font-weight: 600; }"
                "#successBanner { background: #143d25; border: 1px solid #287a3e; color: #89d185; border-radius: 6px; padding: 8px; }"
                "QGroupBox { border: 1px solid #3c3c3c; border-radius: 6px; margin-top: 10px; color: #d4d4d4; }"
                "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #d4d4d4; }"
                "QCheckBox, QRadioButton { color: #d4d4d4; }"
                "QPushButton { min-height: 26px; background: #3c3c3c; color: #cccccc; border: 1px solid #555555; border-radius: 4px; padding: 4px 10px; }"
                "QPushButton:hover { background: #4b4b4b; border-color: #6b6b6b; }"
                "QPushButton:disabled { background: #2d2d30; color: #6a6a6a; border-color: #3c3c3c; }"
                "QPushButton#primaryButton { background: #0e639c; color: #ffffff; border: 0; border-radius: 4px; padding: 8px 16px; }"
                "QPushButton#primaryButton:hover { background: #1177bb; }"
                "QPushButton#primaryButton:disabled { background: #264f78; color: #9fbdd2; }"
                "QListWidget { background: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; border-radius: 6px; }"
                "QListWidget::item { padding: 3px 4px; }"
                "QListWidget::item:selected { background: #264f78; color: #ffffff; }"
                "QListWidget::item:hover { background: #2a2d2e; }"
                "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; selection-background-color: #264f78; selection-color: #ffffff; border: 1px solid #3c3c3c; border-radius: 6px; }"
                "QComboBox { background: #3c3c3c; color: #cccccc; border: 1px solid #555555; border-radius: 4px; padding: 4px 8px; }"
                "QComboBox:hover { border-color: #6b6b6b; }"
                "QComboBox QAbstractItemView { background: #252526; color: #d4d4d4; selection-background-color: #264f78; selection-color: #ffffff; border: 1px solid #3c3c3c; }"));
            m_applyingThemeStyleSheet = false;
            return;
        }

        setStyleSheet(QStringLiteral(
            "#thuCompilerView { background: #ffffff; }"
            "#modeCard { border: 1px solid #d9dde7; border-radius: 8px; background: #ffffff; }"
            "#modeCard:hover { border-color: #6d3cc8; background: #fbf8ff; }"
            "#modeTitle { font-weight: 600; }"
            "#modeGlyph { color: #6d3cc8; font-size: 42px; font-weight: 600; }"
            "#sidePanel { border-left: 1px solid #d9dde7; background: #fbfbfd; }"
            "#outputTitle { font-weight: 600; }"
            "#successBanner { background: #eef8ee; border: 1px solid #cae8ca; color: #2b8a3e; border-radius: 6px; padding: 8px; }"
            "QPushButton { min-height: 26px; }"
            "QPushButton#primaryButton { background: #673ab7; color: white; border: 0; border-radius: 4px; padding: 8px 16px; }"
            "QPushButton#primaryButton:disabled { background: #b8acd2; }"
            "QListWidget { border: 1px solid #d9dde7; border-radius: 6px; }"
            "QListWidget::item:selected { background: #f6edbd; color: #222; }"
            "QPlainTextEdit { border: 1px solid #d9dde7; border-radius: 6px; }"));
        m_applyingThemeStyleSheet = false;
    }

    void scheduleThemeStyleSheetUpdate()
    {
        if (m_themeStyleSheetUpdatePending) {
            return;
        }

        m_themeStyleSheetUpdatePending = true;
        QTimer::singleShot(0, this, [this] {
            m_themeStyleSheetUpdatePending = false;
            applyThemeStyleSheet();
        });
    }

    void setupOptionPages()
    {
        auto makePotPage = [this](QComboBox* combo, const QString& labelText) {
            auto* page = new QWidget(this);
            auto* layout = new QVBoxLayout(page);
            combo->addItem(QStringLiteral("1024"), QStringLiteral("1024"));
            combo->addItem(QStringLiteral("65536"), QStringLiteral("65536"));
            layout->addWidget(new QLabel(labelText, page));
            layout->addWidget(combo);
            layout->addStretch();
            return page;
        };

        auto* thcPage = new QWidget(this);
        auto* thcLayout = new QVBoxLayout(thcPage);
        m_optCombo->addItem(QStringLiteral("-O1 (优化等级 1)"), QStringLiteral("-O1"));
        m_optCombo->addItem(QStringLiteral("-O2 (优化等级 2)"), QStringLiteral("-O2"));
        thcLayout->addWidget(new QLabel(QStringLiteral("优化等级"), thcPage));
        thcLayout->addWidget(m_optCombo);
        auto* genMiddle = new QCheckBox(QStringLiteral("保留中间文件"), thcPage);
        genMiddle->setEnabled(false);
        auto* showDetails = new QCheckBox(QStringLiteral("显示编译详情"), thcPage);
        showDetails->setChecked(true);
        showDetails->setEnabled(false);
        thcLayout->addWidget(genMiddle);
        thcLayout->addWidget(showDetails);
        thcLayout->addStretch();

        m_optionStack->addWidget(makePotPage(m_fftPotCombo, QStringLiteral("FFT pot_num")));
        m_optionStack->addWidget(makePotPage(m_ifftPotCombo, QStringLiteral("IFFT pot_num")));
        m_optionStack->addWidget(thcPage);
    }

    QString defaultProjectDirectory() const
    {
        const auto projects = KDevelop::ICore::self()->projectController()->projects();
        if (!projects.isEmpty()) {
            return projects.first()->path().toLocalFile();
        }
        const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        return docs.isEmpty() ? QDir::homePath() : docs;
    }

    void readSettings()
    {
        const KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("ThuCompiler"));
        m_compilerRoot = config.readEntry("CompilerRoot", locateCompilerRoot());
        m_outputDirectory = config.readEntry("OutputDirectory", QDir(defaultProjectDirectory()).filePath(QStringLiteral("thu-compiler-output")));
        m_outputDirEdit->setPlainText(QDir::toNativeSeparators(m_outputDirectory));
        m_optCombo->setCurrentIndex(config.readEntry("OptimizationIndex", 0));
    }

    void writeSettings() const
    {
        KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("ThuCompiler"));
        config.writeEntry("CompilerRoot", m_compilerRoot);
        config.writeEntry("OutputDirectory", normalizedOutputDirectory());
        config.writeEntry("OptimizationIndex", m_optCombo->currentIndex());
        config.sync();
    }

    QString locateCompilerRoot() const
    {
        const QString envRoot = QString::fromLocal8Bit(qgetenv("THU_COMPILER_ROOT")).trimmed();
        if (isCompilerRoot(envRoot)) {
            return QDir::cleanPath(envRoot);
        }

        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            QDir(appDir).absoluteFilePath(QStringLiteral("../thu-compiler")),
            QDir(appDir).absoluteFilePath(QStringLiteral("../../thu-compiler")),
            QDir(QDir::currentPath()).absoluteFilePath(QStringLiteral("../thu-compiler")),
        };
        for (const QString& candidate : candidates) {
            if (isCompilerRoot(candidate)) {
                return QDir::cleanPath(candidate);
            }
        }
        return {};
    }

    static bool isCompilerRoot(const QString& path)
    {
        return !path.isEmpty() && QFileInfo::exists(QDir(path).filePath(QStringLiteral("compile.bat")));
    }

    QString compileBatPath() const
    {
        return QDir(m_compilerRoot).filePath(QStringLiteral("compile.bat"));
    }

    QString normalizedOutputDirectory() const
    {
        QString text = m_outputDirEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            text = QDir(defaultProjectDirectory()).filePath(QStringLiteral("thu-compiler-output"));
        }
        return QDir::cleanPath(QDir::fromNativeSeparators(text));
    }

    static QString quoteCmdArg(const QString& value)
    {
        QString escaped = QDir::toNativeSeparators(value);
        escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + escaped + QLatin1Char('"');
    }

private Q_SLOTS:
    void addFiles()
    {
        const QStringList files = QFileDialog::getOpenFileNames(this,
                                                                QStringLiteral("选择 C 源文件"),
                                                                defaultProjectDirectory(),
                                                                QStringLiteral("C Source (*.c);;All Files (*)"));
        for (const QString& file : files) {
            addFile(file);
        }
        if (m_fileList->currentRow() < 0 && m_fileList->count() > 0) {
            m_fileList->setCurrentRow(0);
        }
    }

    void addFile(const QString& filePath)
    {
        const QString clean = QDir::cleanPath(filePath);
        for (int i = 0; i < m_fileList->count(); ++i) {
            if (m_fileList->item(i)->data(Qt::UserRole).toString().compare(clean, Qt::CaseInsensitive) == 0) {
                return;
            }
        }

        auto* item = new QListWidgetItem(QFileInfo(clean).fileName(), m_fileList);
        item->setToolTip(QDir::toNativeSeparators(clean));
        item->setData(Qt::UserRole, clean);
    }

    void removeSelectedFile()
    {
        delete m_fileList->takeItem(m_fileList->currentRow());
    }

    void loadSelectedFile()
    {
        const QString filePath = currentFilePath();
        m_editor->clear();
        if (filePath.isEmpty()) {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            appendOutput(QStringLiteral("[ERROR] 无法打开文件：%1\n").arg(file.errorString()));
            return;
        }
        m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    }

    bool saveCurrentFile()
    {
        const QString filePath = currentFilePath();
        if (filePath.isEmpty()) {
            return true;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            appendOutput(QStringLiteral("[ERROR] 无法保存文件：%1\n").arg(file.errorString()));
            return false;
        }

        QTextStream stream(&file);
        stream << m_editor->toPlainText();
        appendOutput(QStringLiteral("[INFO] 已保存：%1\n").arg(QDir::toNativeSeparators(filePath)));
        return true;
    }

    void formatCurrentFile()
    {
        QString text = m_editor->toPlainText();
        text.replace(QStringLiteral("\t"), QStringLiteral("    "));
        m_editor->setPlainText(text);
    }

    void chooseCompilerRoot()
    {
        const QString directory = QFileDialog::getExistingDirectory(this,
                                                                    QStringLiteral("选择 THU Compiler 目录"),
                                                                    m_compilerRoot.isEmpty() ? defaultProjectDirectory() : m_compilerRoot);
        if (directory.isEmpty()) {
            return;
        }
        m_compilerRoot = QDir::cleanPath(directory);
        updateCompilerRootLabel();
        writeSettings();
    }

    void chooseOutputDirectory()
    {
        const QString directory = QFileDialog::getExistingDirectory(this,
                                                                    QStringLiteral("选择输出目录"),
                                                                    normalizedOutputDirectory());
        if (directory.isEmpty()) {
            return;
        }
        m_outputDirectory = QDir::cleanPath(directory);
        m_outputDirEdit->setPlainText(QDir::toNativeSeparators(m_outputDirectory));
        writeSettings();
    }

    void compile()
    {
        if (m_process->state() != QProcess::NotRunning) {
            appendOutput(QStringLiteral("[WARN] 编译进程正在运行。\n"));
            return;
        }
        if (!isCompilerRoot(m_compilerRoot)) {
            appendOutput(QStringLiteral("[ERROR] 未找到 THU compiler compile.bat，请设置编译器目录。\n"));
            return;
        }
        if (!saveCurrentFile()) {
            return;
        }

        const QString outputDir = normalizedOutputDirectory();
        QDir().mkpath(outputDir);
        if (!QFileInfo(outputDir).isDir()) {
            appendOutput(QStringLiteral("[ERROR] 输出目录无效：%1\n").arg(QDir::toNativeSeparators(outputDir)));
            return;
        }

        m_expectedHeaderPaths.clear();
        QStringList args;
        if (m_mode == CompileMode::Fft) {
            args = {QStringLiteral("/c"), QStringLiteral("call"), compileBatPath(), QStringLiteral("-FFT"), m_fftPotCombo->currentData().toString()};
        } else if (m_mode == CompileMode::Ifft) {
            args = {QStringLiteral("/c"), QStringLiteral("call"), compileBatPath(), QStringLiteral("-IFFT"), m_ifftPotCombo->currentData().toString()};
        } else {
            QStringList files;
            for (int i = 0; i < m_fileList->count(); ++i) {
                const QString file = m_fileList->item(i)->data(Qt::UserRole).toString();
                if (!QFileInfo::exists(file)) {
                    appendOutput(QStringLiteral("[ERROR] 文件不存在：%1\n").arg(QDir::toNativeSeparators(file)));
                    return;
                }
                files << file;
            }
            if (files.isEmpty()) {
                appendOutput(QStringLiteral("[ERROR] THC 模式需要至少添加一个 .c 文件。\n"));
                return;
            }
            const QString opt = m_optCombo->currentData().toString();
            m_compileScriptPath = QDir(outputDir).filePath(QStringLiteral(".rrise_thu_compile.cmd"));

            QStringList scriptLines;
            scriptLines << QStringLiteral("@echo off");
            scriptLines << QStringLiteral("setlocal");
            for (const QString& file : std::as_const(files)) {
                const QString headerPath = QDir(outputDir).filePath(QFileInfo(file).completeBaseName() + QStringLiteral(".h"));
                m_expectedHeaderPaths << headerPath;
                scriptLines << QStringLiteral("call %1 -THC %2 %3").arg(quoteCmdArg(compileBatPath()), opt, quoteCmdArg(file));
                scriptLines << QStringLiteral("if errorlevel 1 exit /b %errorlevel%");
                scriptLines << QStringLiteral("if not exist %1 (").arg(quoteCmdArg(headerPath));
                scriptLines << QStringLiteral("  echo ERROR: expected header not generated: %1").arg(QDir::toNativeSeparators(headerPath));
                scriptLines << QStringLiteral("  exit /b 1");
                scriptLines << QStringLiteral(")");
            }
            scriptLines << QStringLiteral("exit /b 0");

            QFile scriptFile(m_compileScriptPath);
            if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                appendOutput(QStringLiteral("[ERROR] 鏃犳硶鍒涘缓 THC 缂栬瘧鑴氭湰锛?1\n").arg(scriptFile.errorString()));
                return;
            }
            scriptFile.write(scriptLines.join(QStringLiteral("\r\n")).toLocal8Bit());
            scriptFile.write("\r\n");
            scriptFile.close();

            args = {QStringLiteral("/d"), QStringLiteral("/c"), QDir::toNativeSeparators(m_compileScriptPath)};
        }

        appendOutput(QStringLiteral("\n[INFO] 编译器：%1\n").arg(QDir::toNativeSeparators(compileBatPath())));
        appendOutput(QStringLiteral("[INFO] 工作目录：%1\n").arg(QDir::toNativeSeparators(outputDir)));
        appendOutput(QStringLiteral("[INFO] 命令：cmd.exe %1\n").arg(args.join(QLatin1Char(' '))));

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        QStringList pathPrefixes;
        const QString texConvertorDir = QDir(m_compilerRoot).filePath(QStringLiteral("tex-convertor"));
        if (QFileInfo(texConvertorDir).isDir()) {
            pathPrefixes << QDir::toNativeSeparators(texConvertorDir);
        }
        const QString llvmBin = QDir(m_compilerRoot).filePath(QStringLiteral("llvm/bin"));
        if (QFileInfo(llvmBin).isDir()) {
            pathPrefixes << QDir::toNativeSeparators(llvmBin);
            appendOutput(QStringLiteral("[INFO] LLVM tools: %1\n").arg(QDir::toNativeSeparators(llvmBin)));
        }
        if (!pathPrefixes.isEmpty()) {
            const QString path = environment.value(QStringLiteral("PATH"));
            environment.insert(QStringLiteral("PATH"), pathPrefixes.join(QLatin1Char(';')) + QLatin1Char(';') + path);
        }

        m_process->setProgram(QStringLiteral("cmd.exe"));
        m_process->setArguments(args);
        m_process->setWorkingDirectory(outputDir);
        m_process->setProcessEnvironment(environment);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        m_compileButton->setEnabled(false);
        m_compileButton->setText(QStringLiteral("编译中..."));
        m_process->start();
    }

    void readProcessOutput()
    {
        appendOutput(QString::fromLocal8Bit(m_process->readAll()));
    }

    void processFinished(int exitCode, QProcess::ExitStatus exitStatus)
    {
        bool ok = exitStatus == QProcess::NormalExit && exitCode == 0;
        QStringList missingHeaders;
        if (ok) {
            for (const QString& headerPath : std::as_const(m_expectedHeaderPaths)) {
                if (!QFileInfo::exists(headerPath)) {
                    missingHeaders << headerPath;
                }
            }
        }
        if (!missingHeaders.isEmpty()) {
            ok = false;
            appendOutput(QStringLiteral("[ERROR] THC header files were not generated:\n"));
            for (const QString& headerPath : std::as_const(missingHeaders)) {
                appendOutput(QStringLiteral("  %1\n").arg(QDir::toNativeSeparators(headerPath)));
            }
        }
        appendOutput(ok ? QStringLiteral("[INFO] 编译成功。\n") : QStringLiteral("[ERROR] 编译失败，退出码：%1\n").arg(exitCode));
        removeCompileScript();
        setStatus(ok);
        m_compileButton->setEnabled(true);
        m_compileButton->setText(QStringLiteral("开始编译"));
        writeSettings();
    }

    void processError(QProcess::ProcessError error)
    {
        Q_UNUSED(error);
        appendOutput(QStringLiteral("[ERROR] 无法启动编译进程：%1\n").arg(m_process->errorString()));
        m_compileButton->setEnabled(true);
        m_compileButton->setText(QStringLiteral("开始编译"));
        removeCompileScript();
        setStatus(false);
    }

    void openOutputDirectory()
    {
        const QString outputDir = normalizedOutputDirectory();
        QDir().mkpath(outputDir);
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QDir::toNativeSeparators(outputDir)});
    }

    void removeCompileScript()
    {
        if (!m_compileScriptPath.isEmpty()) {
            QFile::remove(m_compileScriptPath);
            m_compileScriptPath.clear();
        }
    }

    void toggleFullScreen()
    {
        if (m_fullscreenDialog) {
            exitFullScreen();
        } else {
            enterFullScreen();
        }
    }

private:
    void enterFullScreen()
    {
        if (m_fullscreenDialog || !m_contentWidget || !m_rootLayout) {
            return;
        }

        m_fullscreenDialog = new QDialog(this);
        m_fullscreenDialog->setWindowTitle(QStringLiteral("THU Compiler"));
        auto* layout = new QHBoxLayout(m_fullscreenDialog);
        layout->setContentsMargins(12, 12, 12, 12);

        m_rootLayout->removeWidget(m_contentWidget);
        m_contentWidget->setParent(m_fullscreenDialog);
        layout->addWidget(m_contentWidget, 1);
        m_fullscreenButton->setText(QStringLiteral("退出全屏"));

        connect(m_fullscreenDialog, &QDialog::finished, this, [this]() {
            if (m_fullscreenDialog) {
                exitFullScreen();
            }
        });
        m_fullscreenDialog->showFullScreen();
    }

    void exitFullScreen()
    {
        if (!m_fullscreenDialog || !m_contentWidget || !m_rootLayout) {
            return;
        }

        QDialog* const dialog = m_fullscreenDialog;
        m_fullscreenDialog = nullptr;
        if (auto* dialogLayout = dialog->layout()) {
            dialogLayout->removeWidget(m_contentWidget);
        }
        m_contentWidget->setParent(this);
        m_rootLayout->addWidget(m_contentWidget, 1);
        m_fullscreenButton->setText(QStringLiteral("全屏"));
        dialog->hide();
        dialog->deleteLater();
    }

    QString currentFilePath() const
    {
        auto* item = m_fileList->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    }

    void updateCompilerRootLabel()
    {
        if (isCompilerRoot(m_compilerRoot)) {
            m_compilerRootLabel->setText(QDir::toNativeSeparators(m_compilerRoot));
        } else {
            m_compilerRootLabel->setText(QStringLiteral("未找到 compile.bat"));
        }
    }

    void updateModeUi()
    {
        m_optionStack->setCurrentIndex(static_cast<int>(m_mode));
        const bool thc = m_mode == CompileMode::Thc;
        m_fileList->setEnabled(thc);
        m_editor->setEnabled(thc);
        m_addFileButton->setEnabled(thc);
        m_removeFileButton->setEnabled(thc);
        m_saveButton->setEnabled(thc);
        m_formatButton->setEnabled(thc);
    }

    void appendOutput(const QString& text)
    {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(text);
        m_output->moveCursor(QTextCursor::End);
    }

    void setStatus(bool success)
    {
        const QString outputDir = QDir::toNativeSeparators(normalizedOutputDirectory());
        if (success) {
            m_statusLabel->setText(QStringLiteral("✓ 编译成功    输出目录：%1").arg(outputDir));
            m_statusLabel->setObjectName(QStringLiteral("successBanner"));
        } else {
            m_statusLabel->setText(QStringLiteral("编译失败，请查看输出日志。"));
            m_statusLabel->setObjectName(QString());
        }
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
    }

    CompileMode m_mode = CompileMode::Thc;
    QButtonGroup* const m_modeGroup;
    QListWidget* const m_fileList;
    QPlainTextEdit* const m_editor;
    QPlainTextEdit* const m_output;
    QLabel* const m_statusLabel;
    QStackedWidget* const m_optionStack;
    QComboBox* const m_optCombo;
    QComboBox* const m_fftPotCombo;
    QComboBox* const m_ifftPotCombo;
    QPlainTextEdit* const m_outputDirEdit;
    QLabel* const m_compilerRootLabel;
    QPushButton* const m_compileButton;
    QPushButton* const m_saveButton;
    QPushButton* const m_formatButton;
    QPushButton* const m_addFileButton;
    QPushButton* const m_removeFileButton;
    QPushButton* const m_clearOutputButton;
    QPushButton* const m_copyOutputButton;
    QPushButton* const m_openOutputDirButton;
    QPushButton* const m_fullscreenButton;
    QPushButton* const m_changeCompilerButton;
    QPushButton* const m_chooseOutputDirButton;
    QProcess* const m_process;
    QWidget* m_contentWidget = nullptr;
    QHBoxLayout* m_rootLayout = nullptr;
    QDialog* m_fullscreenDialog = nullptr;
    bool m_applyingThemeStyleSheet = false;
    bool m_themeStyleSheetUpdatePending = false;
    QString m_compilerRoot;
    QString m_outputDirectory;
    QString m_compileScriptPath;
    QStringList m_expectedHeaderPaths;
};

class ThuCompilerPlugin : public KDevelop::IPlugin
{
    Q_OBJECT

public:
    ThuCompilerPlugin(QObject* parent, const KPluginMetaData& metaData, const QVariantList&)
        : KDevelop::IPlugin(QStringLiteral("kdevthucompiler"), parent, metaData)
        , m_factory(new ThuCompilerToolViewFactory)
    {
        const QString toolViewName = QStringLiteral("THU Compiler");
        core()->uiController()->addToolView(toolViewName, m_factory, KDevelop::IUiController::CreateAndRaise);
        auto raiseThuCompilerView = [this, toolViewName]() {
            core()->uiController()->findToolView(toolViewName, m_factory, KDevelop::IUiController::CreateAndRaise);
        };
        QTimer::singleShot(0, this, raiseThuCompilerView);
    }

    void unload() override
    {
        core()->uiController()->removeToolView(m_factory);
    }

private:
    class ThuCompilerToolViewFactory : public KDevelop::IToolViewFactory
    {
    public:
        QWidget* create(QWidget* parent = nullptr) override
        {
            return new ThuCompilerView(parent);
        }

        Qt::DockWidgetArea defaultPosition() const override
        {
            return Qt::BottomDockWidgetArea;
        }

        QString id() const override
        {
            return QStringLiteral("org.kdevelop.ThuCompiler");
        }
    };

    ThuCompilerToolViewFactory* const m_factory;
};

K_PLUGIN_FACTORY_WITH_JSON(ThuCompilerFactory, "kdevthucompiler.json", registerPlugin<ThuCompilerPlugin>();)

#include "thucompilerplugin.moc"
