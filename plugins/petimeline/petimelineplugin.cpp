/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "petimelinemodel.h"

#include <interfaces/icore.h>
#include <interfaces/iplugin.h>
#include <interfaces/iuicontroller.h>

#include <KActionCollection>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

class TimelineCanvas : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit TimelineCanvas(QWidget* parent = nullptr)
        : QAbstractScrollArea(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setFocusPolicy(Qt::StrongFocus);
        viewport()->setMouseTracking(true);
        horizontalScrollBar()->setSingleStep(80);
        verticalScrollBar()->setSingleStep(m_rowHeight);
    }

    void setModel(const PeTimeline::Model* model)
    {
        m_model = model;
        m_selectedId = -1;
        updateScrollBars();
        viewport()->update();
    }

    void setSearchText(const QString& text)
    {
        m_search = text.trimmed();
        viewport()->update();
    }

    void setConflictsOnly(bool enabled)
    {
        m_conflictsOnly = enabled;
        viewport()->update();
    }

    void fitTimeline()
    {
        if (!m_model || m_model->maxCycle <= 0)
            return;
        const double available = std::max(40, viewport()->width() - m_labelWidth - 16);
        m_pixelsPerCycle = std::clamp(available / static_cast<double>(m_model->maxCycle), 0.15, 80.0);
        horizontalScrollBar()->setValue(0);
        updateScrollBars();
        viewport()->update();
    }

Q_SIGNALS:
    void eventSelected(int id);

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::TextAntialiasing);
        const QPalette colors = palette();
        painter.fillRect(viewport()->rect(), colors.color(QPalette::Base));
        if (!m_model) {
            painter.setPen(colors.color(QPalette::PlaceholderText));
            painter.drawText(viewport()->rect(), Qt::AlignCenter,
                             QStringLiteral("打开 GReP/MUSIC .tex 文件开始时序分析"));
            return;
        }

        const int horizontal = horizontalScrollBar()->value();
        const int vertical = verticalScrollBar()->value();
        const int firstPe = std::max(0, vertical / m_rowHeight);
        const int lastPe = std::min(63, (vertical + viewport()->height() - m_rulerHeight) / m_rowHeight + 1);
        const qint64 firstCycle =
            std::max<qint64>(0, static_cast<qint64>((horizontal - m_labelWidth) / m_pixelsPerCycle));
        const qint64 lastCycle = static_cast<qint64>((horizontal + viewport()->width()) / m_pixelsPerCycle) + 1;

        QColor rowAlternate = colors.color(QPalette::AlternateBase);
        for (int pe = firstPe; pe <= lastPe; ++pe) {
            const int y = m_rulerHeight + pe * m_rowHeight - vertical;
            if (pe % 2)
                painter.fillRect(0, y, viewport()->width(), m_rowHeight, rowAlternate);
            painter.setPen(blend(colors.color(QPalette::Mid), colors.color(QPalette::Base), 0.45));
            painter.drawLine(0, y + m_rowHeight - 1, viewport()->width(), y + m_rowHeight - 1);
        }

        const qint64 gridStep = cycleGridStep();
        const qint64 gridStart = (firstCycle / gridStep) * gridStep;
        painter.setPen(blend(colors.color(QPalette::Mid), colors.color(QPalette::Base), 0.55));
        for (qint64 cycle = gridStart; cycle <= lastCycle; cycle += gridStep) {
            const int x = m_labelWidth + cycleToPixel(cycle) - horizontal;
            painter.drawLine(x, m_rulerHeight, x, viewport()->height());
        }

        for (int pe = firstPe; pe <= lastPe; ++pe) {
            const int y = m_rulerHeight + pe * m_rowHeight - vertical + 3;
            for (const auto& event : m_model->tracks.at(pe).events) {
                if (event.start + event.duration < firstCycle || event.start > lastCycle || !matches(event))
                    continue;
                const int x = m_labelWidth + cycleToPixel(event.start) - horizontal;
                const int width = std::max(2, cycleToPixel(event.start + event.duration) - cycleToPixel(event.start));
                const QRect rect(x, y, width, m_rowHeight - 6);
                QColor fill = eventColor(event.category);
                painter.setBrush(fill);
                QColor outline = blend(fill, Qt::black, 0.35);
                int penWidth = 1;
                if (!event.conflicts.isEmpty()) {
                    outline = QColor(225, 54, 54);
                    penWidth = 2;
                } else if (!event.advisories.isEmpty()) {
                    outline = QColor(238, 151, 35);
                    penWidth = 2;
                }
                if (event.id == m_selectedId) {
                    outline = colors.color(QPalette::Highlight);
                    penWidth = 3;
                }
                painter.setPen(QPen(outline, penWidth));
                painter.drawRect(rect.adjusted(0, 0, -1, -1));
                if (width >= 22) {
                    painter.setPen(fill.lightness() < 135 ? Qt::white : QColor(28, 31, 35));
                    painter.drawText(rect.adjusted(4, 0, -3, 0), Qt::AlignVCenter | Qt::AlignLeft,
                                     painter.fontMetrics().elidedText(event.name, Qt::ElideRight, width - 7));
                }
            }
        }

        painter.fillRect(0, 0, viewport()->width(), m_rulerHeight, colors.color(QPalette::Window));
        painter.setPen(colors.color(QPalette::WindowText));
        for (qint64 cycle = gridStart; cycle <= lastCycle; cycle += gridStep) {
            const int x = m_labelWidth + cycleToPixel(cycle) - horizontal;
            painter.drawLine(x, m_rulerHeight - 5, x, m_rulerHeight);
            painter.drawText(QRect(x + 3, 0, 100, m_rulerHeight - 4), Qt::AlignLeft | Qt::AlignVCenter,
                             QString::number(cycle));
        }
        painter.fillRect(0, 0, m_labelWidth, viewport()->height(), colors.color(QPalette::Window));
        painter.setPen(colors.color(QPalette::WindowText));
        painter.drawText(QRect(0, 0, m_labelWidth, m_rulerHeight), Qt::AlignCenter, QStringLiteral("周期"));
        for (int pe = firstPe; pe <= lastPe; ++pe) {
            const int y = m_rulerHeight + pe * m_rowHeight - vertical;
            painter.drawText(QRect(0, y, m_labelWidth - 7, m_rowHeight), Qt::AlignRight | Qt::AlignVCenter,
                             QStringLiteral("PE%1").arg(pe));
        }
        painter.setPen(colors.color(QPalette::Mid));
        painter.drawLine(m_labelWidth - 1, 0, m_labelWidth - 1, viewport()->height());
        painter.drawLine(0, m_rulerHeight - 1, viewport()->width(), m_rulerHeight - 1);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            if (const auto* hit = eventAt(event->position().toPoint())) {
                m_selectedId = hit->id;
                Q_EMIT eventSelected(hit->id);
                viewport()->update();
                return;
            }
            m_dragStart = event->position().toPoint();
            m_dragScrollStart = horizontalScrollBar()->value();
            m_dragging = true;
            viewport()->setCursor(Qt::ClosedHandCursor);
        }
        QAbstractScrollArea::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_dragging) {
            horizontalScrollBar()->setValue(m_dragScrollStart - (event->position().toPoint().x() - m_dragStart.x()));
            return;
        }
        if (const auto* hit = eventAt(event->position().toPoint())) {
            viewport()->setCursor(Qt::PointingHandCursor);
            viewport()->setToolTip(QStringLiteral("PE%1  周期 %2-%3\n%4\n源代码行 %5")
                                       .arg(hit->pe)
                                       .arg(hit->start)
                                       .arg(hit->start + hit->duration)
                                       .arg(hit->source)
                                       .arg(hit->line));
        } else {
            viewport()->unsetCursor();
            viewport()->setToolTip(QString());
        }
        QAbstractScrollArea::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            viewport()->unsetCursor();
        }
        QAbstractScrollArea::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers().testFlag(Qt::ControlModifier) && m_model) {
            const int x = event->position().toPoint().x();
            const double anchorCycle = (horizontalScrollBar()->value() + x - m_labelWidth) / m_pixelsPerCycle;
            const double factor = std::pow(1.18, event->angleDelta().y() / 120.0);
            m_pixelsPerCycle = std::clamp(m_pixelsPerCycle * factor, 0.15, 80.0);
            updateScrollBars();
            horizontalScrollBar()->setValue(qRound(anchorCycle * m_pixelsPerCycle - x + m_labelWidth));
            viewport()->update();
            event->accept();
            return;
        }
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->angleDelta().y());
            event->accept();
            return;
        }
        QAbstractScrollArea::wheelEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QAbstractScrollArea::resizeEvent(event);
        updateScrollBars();
    }

private:
    static QColor blend(const QColor& first, const QColor& second, double secondWeight)
    {
        return QColor::fromRgbF(first.redF() * (1.0 - secondWeight) + second.redF() * secondWeight,
                                first.greenF() * (1.0 - secondWeight) + second.greenF() * secondWeight,
                                first.blueF() * (1.0 - secondWeight) + second.blueF() * secondWeight);
    }

    QColor eventColor(const QString& category) const
    {
        if (category == QLatin1String("compute"))
            return QColor(54, 116, 217);
        if (category == QLatin1String("route"))
            return QColor(25, 146, 124);
        if (category == QLatin1String("logic"))
            return QColor(139, 91, 196);
        if (category == QLatin1String("convert"))
            return QColor(39, 142, 178);
        if (category == QLatin1String("control"))
            return QColor(207, 92, 121);
        if (category == QLatin1String("wait"))
            return QColor(224, 174, 54);
        return QColor(82, 151, 77);
    }

    int cycleToPixel(qint64 cycle) const
    {
        return static_cast<int>(std::min<double>(std::numeric_limits<int>::max() / 2.0, cycle * m_pixelsPerCycle));
    }

    qint64 cycleGridStep() const
    {
        const double targetCycles = 90.0 / m_pixelsPerCycle;
        const double power = std::pow(10.0, std::floor(std::log10(std::max(1.0, targetCycles))));
        const double normalized = targetCycles / power;
        const double multiplier =
            normalized <= 1.0 ? 1.0 : (normalized <= 2.0 ? 2.0 : (normalized <= 5.0 ? 5.0 : 10.0));
        return std::max<qint64>(1, qRound64(multiplier * power));
    }

    bool matches(const PeTimeline::Event& event) const
    {
        if (m_conflictsOnly && event.conflicts.isEmpty() && event.advisories.isEmpty())
            return false;
        if (m_search.isEmpty())
            return true;
        QString haystack = event.name + QLatin1Char(' ') + event.source + QStringLiteral(" PE%1 ").arg(event.pe);
        for (int address : event.reads)
            haystack += QStringLiteral(" SM%1").arg(address);
        for (int address : event.writes)
            haystack += QStringLiteral(" SM%1").arg(address);
        return haystack.contains(m_search, Qt::CaseInsensitive);
    }

    const PeTimeline::Event* eventAt(const QPoint& position) const
    {
        if (!m_model || position.x() < m_labelWidth || position.y() < m_rulerHeight)
            return nullptr;
        const int pe = (position.y() - m_rulerHeight + verticalScrollBar()->value()) / m_rowHeight;
        if (pe < 0 || pe >= m_model->tracks.size())
            return nullptr;
        const double cycle = (position.x() - m_labelWidth + horizontalScrollBar()->value()) / m_pixelsPerCycle;
        for (const auto& event : m_model->tracks.at(pe).events) {
            if (matches(event) && cycle >= event.start && cycle < event.start + event.duration)
                return &event;
        }
        return nullptr;
    }

    void updateScrollBars()
    {
        const int contentWidth = m_model ? m_labelWidth + cycleToPixel(m_model->maxCycle) + 20 : 0;
        const int contentHeight = m_rulerHeight + 64 * m_rowHeight;
        horizontalScrollBar()->setPageStep(viewport()->width());
        horizontalScrollBar()->setRange(0, std::max(0, contentWidth - viewport()->width()));
        verticalScrollBar()->setPageStep(viewport()->height());
        verticalScrollBar()->setRange(0, std::max(0, contentHeight - viewport()->height()));
    }

    const PeTimeline::Model* m_model = nullptr;
    QString m_search;
    double m_pixelsPerCycle = 12.0;
    int m_selectedId = -1;
    int m_labelWidth = 58;
    int m_rulerHeight = 28;
    int m_rowHeight = 28;
    bool m_conflictsOnly = false;
    bool m_dragging = false;
    QPoint m_dragStart;
    int m_dragScrollStart = 0;
};

class PeTimelineView : public QWidget
{
    Q_OBJECT

public:
    explicit PeTimelineView(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_canvas(new TimelineCanvas(this))
        , m_details(new QPlainTextEdit(this))
        , m_fileLabel(new QLabel(QStringLiteral("尚未打开文件"), this))
        , m_statsLabel(new QLabel(this))
        , m_searchEdit(new QLineEdit(this))
        , m_peaSpin(new QSpinBox(this))
        , m_bankSpin(new QSpinBox(this))
        , m_clockSpin(new QDoubleSpinBox(this))
        , m_layoutCombo(new QComboBox(this))
        , m_waitCombo(new QComboBox(this))
        , m_mccBroadcast(new QCheckBox(QStringLiteral("允许 MCC 同址广播"), this))
        , m_conflictsOnly(new QCheckBox(QStringLiteral("仅显示风险"), this))
    {
        buildUi();
        readSettings();
        connect(m_canvas, &TimelineCanvas::eventSelected, this, &PeTimelineView::showEventDetails);
        connect(m_searchEdit, &QLineEdit::textChanged, m_canvas, &TimelineCanvas::setSearchText);
        connect(m_conflictsOnly, &QCheckBox::toggled, m_canvas, &TimelineCanvas::setConflictsOnly);
    }

    ~PeTimelineView() override
    {
        writeSettings();
    }

private:
    QToolButton* toolButton(QStyle::StandardPixmap icon, const QString& text, const QString& tooltip)
    {
        auto* button = new QToolButton(this);
        button->setIcon(style()->standardIcon(icon));
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setToolTip(tooltip);
        return button;
    }

    void buildUi()
    {
        auto* openButton =
            toolButton(QStyle::SP_DialogOpenButton, QStringLiteral("打开"), QStringLiteral("打开 LaTeX 时序配置"));
        auto* reloadButton =
            toolButton(QStyle::SP_BrowserReload, QStringLiteral("重新分析"), QStringLiteral("重新读取并分析当前文件"));
        auto* exportButton =
            toolButton(QStyle::SP_DialogSaveButton, QStringLiteral("导出 JSON"), QStringLiteral("导出结构化时序模型"));
        auto* fitButton =
            toolButton(QStyle::SP_DesktopIcon, QStringLiteral("适应宽度"), QStringLiteral("缩放到完整时序"));

        m_searchEdit->setPlaceholderText(QStringLiteral("搜索指令、PE 或 SM 地址"));
        m_searchEdit->setClearButtonEnabled(true);
        m_searchEdit->setMaximumWidth(280);
        m_peaSpin->setRange(0, 255);
        m_bankSpin->setRange(1, 4096);
        m_bankSpin->setValue(64);
        m_clockSpin->setRange(0.001, 1000000.0);
        m_clockSpin->setDecimals(3);
        m_clockSpin->setSuffix(QStringLiteral(" ns"));
        m_clockSpin->setValue(10.0);
        m_layoutCombo->addItem(QStringLiteral("PEA64 低/高 32 Bank"), true);
        m_layoutCombo->addItem(QStringLiteral("地址取模"), false);
        m_waitCombo->addItem(QStringLiteral("编译器头字段"), true);
        m_waitCombo->addItem(QStringLiteral("普通等待事件"), false);
        m_mccBroadcast->setChecked(true);
        m_details->setReadOnly(true);
        m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_details->setPlaceholderText(QStringLiteral("点击时序事件查看源代码、读写地址和冲突详情"));
        m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_fileLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

        auto* commandRow = new QHBoxLayout;
        commandRow->setContentsMargins(0, 0, 0, 0);
        commandRow->addWidget(openButton);
        commandRow->addWidget(reloadButton);
        commandRow->addWidget(exportButton);
        commandRow->addWidget(fitButton);
        commandRow->addSpacing(8);
        commandRow->addWidget(m_fileLabel, 1);
        commandRow->addWidget(m_searchEdit);
        commandRow->addWidget(m_conflictsOnly);

        auto* optionsRow = new QHBoxLayout;
        optionsRow->setContentsMargins(0, 0, 0, 0);
        optionsRow->addWidget(new QLabel(QStringLiteral("PEA"), this));
        optionsRow->addWidget(m_peaSpin);
        optionsRow->addWidget(new QLabel(QStringLiteral("Bank 布局"), this));
        optionsRow->addWidget(m_layoutCombo);
        optionsRow->addWidget(new QLabel(QStringLiteral("Bank 数"), this));
        optionsRow->addWidget(m_bankSpin);
        optionsRow->addWidget(new QLabel(QStringLiteral("时钟"), this));
        optionsRow->addWidget(m_clockSpin);
        optionsRow->addWidget(new QLabel(QStringLiteral("首条 Wait"), this));
        optionsRow->addWidget(m_waitCombo);
        optionsRow->addWidget(m_mccBroadcast);
        optionsRow->addStretch();
        optionsRow->addWidget(m_statsLabel);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->addWidget(m_canvas);
        splitter->addWidget(m_details);
        splitter->setStretchFactor(0, 4);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({900, 280});

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(5);
        layout->addLayout(commandRow);
        layout->addLayout(optionsRow);
        layout->addWidget(splitter, 1);

        connect(openButton, &QToolButton::clicked, this, &PeTimelineView::openFile);
        connect(reloadButton, &QToolButton::clicked, this, &PeTimelineView::analyzeCurrentFile);
        connect(exportButton, &QToolButton::clicked, this, &PeTimelineView::exportJson);
        connect(fitButton, &QToolButton::clicked, m_canvas, &TimelineCanvas::fitTimeline);
        connect(m_layoutCombo, &QComboBox::currentIndexChanged, this, [this]() {
            const bool pea64 = m_layoutCombo->currentData().toBool();
            if (pea64)
                m_bankSpin->setValue(64);
            m_bankSpin->setEnabled(!pea64);
        });
    }

    PeTimeline::Options options() const
    {
        PeTimeline::Options value;
        value.pea = m_peaSpin->value();
        value.bankCount = m_bankSpin->value();
        value.clockNs = m_clockSpin->value();
        value.pea64BankLayout = m_layoutCombo->currentData().toBool();
        value.compilerFirstWait = m_waitCombo->currentData().toBool();
        value.allowMccBroadcast = m_mccBroadcast->isChecked();
        return value;
    }

    void openFile()
    {
        const QString initial = m_filePath.isEmpty() ? QDir::homePath() : QFileInfo(m_filePath).absolutePath();
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 LaTeX 时序配置"), initial,
                                                          QStringLiteral("LaTeX 文件 (*.tex);;所有文件 (*.*)"));
        if (path.isEmpty())
            return;
        m_filePath = QDir::cleanPath(path);
        analyzeCurrentFile();
    }

    void analyzeCurrentFile()
    {
        if (m_filePath.isEmpty()) {
            openFile();
            return;
        }
        QFile file(m_filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, QStringLiteral("PE 时序分析"),
                                  QStringLiteral("无法读取文件：\n%1\n\n%2")
                                      .arg(QDir::toNativeSeparators(m_filePath), file.errorString()));
            return;
        }
        const QString source = QString::fromUtf8(file.readAll());
        PeTimeline::Model analyzed;
        QString error;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const bool ok = PeTimeline::Analyzer::analyze(source, m_filePath, options(), &analyzed, &error);
        QApplication::restoreOverrideCursor();
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("时序分析失败"), error);
            return;
        }
        m_model = std::move(analyzed);
        m_fileLabel->setText(QDir::toNativeSeparators(m_filePath));
        m_fileLabel->setToolTip(QDir::toNativeSeparators(m_filePath));
        m_statsLabel->setText(QStringLiteral("%1 周期  |  %2 事件  |  %3 冲突  |  %4 提示")
                                  .arg(m_model.maxCycle)
                                  .arg(m_model.eventCount)
                                  .arg(m_model.conflictCount)
                                  .arg(m_model.advisoryCount));
        m_canvas->setModel(&m_model);
        m_canvas->fitTimeline();
        showSummary();
    }

    void showSummary()
    {
        QString text =
            QStringLiteral(
                "文件：%1\nPEA：%2\n最大周期：%3\n估算时间：%4 ns\n事件：%5\n冲突：%6\n提示：%7\nBank：%8 (%9)\n")
                .arg(QDir::toNativeSeparators(m_model.sourceFile))
                .arg(m_model.pea)
                .arg(m_model.maxCycle)
                .arg(m_model.maxCycle * m_model.clockNs, 0, 'f', 3)
                .arg(m_model.eventCount)
                .arg(m_model.conflictCount)
                .arg(m_model.advisoryCount)
                .arg(m_model.bankCount)
                .arg(m_model.bankLayout);
        if (!m_model.warnings.isEmpty())
            text += QStringLiteral("\n分析警告：\n- ") + m_model.warnings.join(QStringLiteral("\n- "));
        m_details->setPlainText(text);
    }

    void showEventDetails(int id)
    {
        const PeTimeline::Event* selected = nullptr;
        for (const auto& track : m_model.tracks) {
            for (const auto& event : track.events) {
                if (event.id == id) {
                    selected = &event;
                    break;
                }
            }
            if (selected)
                break;
        }
        if (!selected)
            return;
        auto addresses = [](const QList<int>& values) {
            QStringList result;
            for (int value : values)
                result << QStringLiteral("SM[%1]").arg(value);
            return result.isEmpty() ? QStringLiteral("无") : result.join(QStringLiteral(", "));
        };
        QString text = QStringLiteral(
                           "%1\n\nPE：%2\n周期：%3 - %4\n持续：%5 cycle (%6 "
                           "ns)\n源代码行：%7\n循环位置：%8\n\n读取：%9\n写入：%10\n\nLaTeX：\n%11")
                           .arg(selected->name)
                           .arg(selected->pe)
                           .arg(selected->start)
                           .arg(selected->start + selected->duration)
                           .arg(selected->duration)
                           .arg(selected->duration * m_model.clockNs, 0, 'f', 3)
                           .arg(selected->line)
                           .arg(selected->loop.isEmpty() ? QStringLiteral("无") : selected->loop)
                           .arg(addresses(selected->reads), addresses(selected->writes), selected->source);
        if (!selected->conflicts.isEmpty())
            text += QStringLiteral("\n\n冲突：\n- ") + selected->conflicts.join(QStringLiteral("\n- "));
        if (!selected->advisories.isEmpty())
            text += QStringLiteral("\n\n分析提示：\n- ") + selected->advisories.join(QStringLiteral("\n- "));
        m_details->setPlainText(text);
    }

    void exportJson()
    {
        if (m_model.tracks.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("导出 JSON"),
                                     QStringLiteral("请先打开并分析一个 .tex 文件。"));
            return;
        }
        const QString suggested = QFileInfo(m_filePath).absolutePath() + QLatin1Char('/')
            + QFileInfo(m_filePath).completeBaseName() + QStringLiteral(".timeline.json");
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出时序模型"), suggested,
                                                          QStringLiteral("JSON 文件 (*.json)"));
        if (path.isEmpty())
            return;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || file.write(PeTimeline::Analyzer::toJson(m_model)) < 0) {
            QMessageBox::critical(this, QStringLiteral("导出失败"),
                                  QStringLiteral("无法写入文件：\n%1").arg(file.errorString()));
        }
    }

    void readSettings()
    {
        KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("PeTimeline"));
        m_filePath = config.readEntry(QStringLiteral("LastFile"), QString());
        m_peaSpin->setValue(config.readEntry(QStringLiteral("Pea"), 0));
        m_clockSpin->setValue(config.readEntry(QStringLiteral("ClockNs"), 10.0));
        m_mccBroadcast->setChecked(config.readEntry(QStringLiteral("AllowMccBroadcast"), true));
        m_layoutCombo->setCurrentIndex(config.readEntry(QStringLiteral("Pea64Layout"), true) ? 0 : 1);
        m_waitCombo->setCurrentIndex(config.readEntry(QStringLiteral("CompilerFirstWait"), true) ? 0 : 1);
        m_bankSpin->setValue(config.readEntry(QStringLiteral("BankCount"), 64));
        m_bankSpin->setEnabled(!m_layoutCombo->currentData().toBool());
    }

    void writeSettings()
    {
        KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("PeTimeline"));
        config.writeEntry(QStringLiteral("LastFile"), m_filePath);
        config.writeEntry(QStringLiteral("Pea"), m_peaSpin->value());
        config.writeEntry(QStringLiteral("ClockNs"), m_clockSpin->value());
        config.writeEntry(QStringLiteral("AllowMccBroadcast"), m_mccBroadcast->isChecked());
        config.writeEntry(QStringLiteral("Pea64Layout"), m_layoutCombo->currentData().toBool());
        config.writeEntry(QStringLiteral("CompilerFirstWait"), m_waitCombo->currentData().toBool());
        config.writeEntry(QStringLiteral("BankCount"), m_bankSpin->value());
        config.sync();
    }

    TimelineCanvas* const m_canvas;
    QPlainTextEdit* const m_details;
    QLabel* const m_fileLabel;
    QLabel* const m_statsLabel;
    QLineEdit* const m_searchEdit;
    QSpinBox* const m_peaSpin;
    QSpinBox* const m_bankSpin;
    QDoubleSpinBox* const m_clockSpin;
    QComboBox* const m_layoutCombo;
    QComboBox* const m_waitCombo;
    QCheckBox* const m_mccBroadcast;
    QCheckBox* const m_conflictsOnly;
    QString m_filePath;
    PeTimeline::Model m_model;
};

class PeTimelinePlugin : public KDevelop::IPlugin
{
    Q_OBJECT

public:
    PeTimelinePlugin(QObject* parent, const KPluginMetaData& metaData, const QVariantList&)
        : KDevelop::IPlugin(QStringLiteral("kdevpetimeline"), parent, metaData)
        , m_factory(new ToolViewFactory)
    {
        setXMLFile(QStringLiteral("kdevpetimeline.rc"));
        core()->uiController()->addToolView(m_toolViewName, m_factory);

        auto* showAction = actionCollection()->addAction(QStringLiteral("show_pe_timeline"));
        showAction->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar-timeline")));
        showAction->setText(QStringLiteral("LaTeX PE 时序图..."));
        showAction->setToolTip(QStringLiteral("打开 GReP/MUSIC LaTeX PE 时序分析面板"));
        connect(showAction, &QAction::triggered, this, &PeTimelinePlugin::showToolView);

        KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("PeTimeline"));
        if (!config.readEntry(QStringLiteral("ToolViewIntroduced"), false)) {
            config.writeEntry(QStringLiteral("ToolViewIntroduced"), true);
            config.sync();
            QTimer::singleShot(700, this, &PeTimelinePlugin::showToolView);
        }
    }

    void unload() override
    {
        core()->uiController()->removeToolView(m_factory);
    }

private:
    void showToolView()
    {
        core()->uiController()->findToolView(m_toolViewName, m_factory, KDevelop::IUiController::CreateAndRaise);
    }

    class ToolViewFactory : public KDevelop::IToolViewFactory
    {
    public:
        QWidget* create(QWidget* parent = nullptr) override
        {
            return new PeTimelineView(parent);
        }
        Qt::DockWidgetArea defaultPosition() const override
        {
            return Qt::BottomDockWidgetArea;
        }
        QString id() const override
        {
            return QStringLiteral("org.kdevelop.PeTimeline");
        }
    };

    ToolViewFactory* const m_factory;
    const QString m_toolViewName = QStringLiteral("LaTeX PE 时序图");
};

}

K_PLUGIN_FACTORY_WITH_JSON(PeTimelineFactory, "kdevpetimeline.json", registerPlugin<PeTimelinePlugin>();)

#include "petimelineplugin.moc"
