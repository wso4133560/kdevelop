/*
    SPDX-FileCopyrightText: 2024 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "welcomepageview.h"

// plugin
#include "ui_welcomepageview.h"
#include "sessionlistmodel.h"
#include "debug.h"
// KDevPlatform
#include <shell/core.h>
#include <shell/uicontroller.h>
#include <shell/sessioncontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <sublime/area.h>
#include <sublime/mainwindow.h>
// KF
#include <KAboutData>
#include <KXMLGUIFactory>
#include <KActionCollection>
#include <KIconLoader>
// Qt
#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QImage>
#include <QPalette>
#include <QSortFilterProxyModel>

namespace {
bool isDarkPalette(const QPalette& palette)
{
    return palette.color(QPalette::Base).lightness() < palette.color(QPalette::Text).lightness();
}

QPixmap readableLogoPixmap(const QIcon& icon, int size, bool dark)
{
    QPixmap pixmap = icon.pixmap(size);
    if (pixmap.isNull() || !dark) {
        return pixmap;
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    constexpr int lightLogoRed = 238;
    constexpr int lightLogoGreen = 242;
    constexpr int lightLogoBlue = 246;

    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = QColor::fromRgba(line[x]);
            if (color.alpha() == 0) {
                continue;
            }

            const int luminance = qGray(color.rgb());
            if (luminance < 180) {
                line[x] = qRgba(lightLogoRed, lightLogoGreen, lightLogoBlue, color.alpha());
            }
        }
    }

    return QPixmap::fromImage(image);
}
}

WelcomePageWidget::WelcomePageWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::WelcomePageView())
    , m_sessionListModel(new SessionListModel(this))
{
    m_ui->setupUi(this);
    updateApplicationLogo();

    m_ui->pageFrame->setBackgroundRole(QPalette::Base);

    auto* sessionsModelSortProxyModel = new QSortFilterProxyModel(this);
    sessionsModelSortProxyModel->setSourceModel(m_sessionListModel);
    sessionsModelSortProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    sessionsModelSortProxyModel->sort(0);

    m_ui->sessionsListView->setModel(sessionsModelSortProxyModel);

    connect(m_ui->welcomeText, &QLabel::linkActivated, this, &WelcomePageWidget::onWelcomeTextLinkClicked);

    onSessionListSizeChanged(m_sessionListModel->size());
    connect(m_sessionListModel, &SessionListModel::sizeChanged, this, &WelcomePageWidget::onSessionListSizeChanged);

    connect(m_ui->sessionsListView, &QAbstractItemView::clicked, this, &WelcomePageWidget::onSessionClicked);

    // track mouse entering indexes and the viewport to control the mouse cursor shape over items
    connect(m_ui->sessionsListView, &QAbstractItemView::entered, this, &WelcomePageWidget::onSessionEntered);
    connect(m_ui->sessionsListView, &QAbstractItemView::viewportEntered, this,
            &WelcomePageWidget::onSessionsViewportEntered);

    connect(m_ui->homepageLinkLabel, &KUrlLabel::leftClickedUrl, this, &WelcomePageWidget::onHomepageClicked);
    connect(m_ui->learnAboutLinkLabel, &KUrlLabel::leftClickedUrl, this, &WelcomePageWidget::onLearnAboutTeamClicked);
    connect(m_ui->joinTeamLinkLabel, &KUrlLabel::leftClickedUrl, this, &WelcomePageWidget::onJoinTeamClicked);
    connect(m_ui->handbookLinkLabel, &KUrlLabel::leftClickedUrl, this, &WelcomePageWidget::onHandbookClicked);

    connect(m_ui->newProjectButton, &QPushButton::clicked, this, &WelcomePageWidget::onNewProjectClicked);
    connect(m_ui->openProjectButton, &QPushButton::clicked, this, &WelcomePageWidget::onOpenProjectClicked);
    connect(m_ui->fetchProjectButton, &QPushButton::clicked, this, &WelcomePageWidget::onFetchProjectClicked);
    connect(m_ui->recentProjectsButton, &QPushButton::clicked, this, &WelcomePageWidget::onRecentProjectsClicked);
}

void WelcomePageWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange) {
        updateApplicationLogo();
    }
}

void WelcomePageWidget::updateApplicationLogo()
{
    const QIcon appIcon = QApplication::windowIcon().isNull()
        ? QIcon::fromTheme(QStringLiteral("kdevelop"))
        : QApplication::windowIcon();
    const int size = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    m_ui->appIconLabel->setPixmap(readableLogoPixmap(appIcon, size, isDarkPalette(palette())));
}

QAction* WelcomePageWidget::mainWindowActionById(const QString& id) const
{
    const auto clients = KDevelop::Core::self()->uiController()->activeMainWindow()->guiFactory()->clients();
    for (auto* const client : clients) {
        auto* const action = client->actionCollection()->action(id);
        if (action) {
            return action;
        }
    }
    return nullptr;
}

void WelcomePageWidget::triggerMainWindowActionById(const QString& id)
{
    auto* const action = mainWindowActionById(id);
    if (!action) {
        qCWarning(PLUGIN_WELCOMEPAGE) << "Action not found in mainwindow:" << id;
        return;
    }

    action->trigger();
}

void WelcomePageWidget::triggerMainWindowActionMenuById(const QString& id)
{
    auto* const action = mainWindowActionById(id);
    if (!action) {
        qCWarning(PLUGIN_WELCOMEPAGE) << "Action not found in mainwindow:" << id;
        return;
    }
    auto* const menu = action->menu();
    if (!menu) {
        qCWarning(PLUGIN_WELCOMEPAGE) << "Action has no menu:" << id;
        return;
    }

    menu->popup(QCursor::pos());
}

void WelcomePageWidget::onWelcomeTextLinkClicked(const QString& link)
{
    QDesktopServices::openUrl(QUrl(link));
}

void WelcomePageWidget::onHomepageClicked()
{
    QDesktopServices::openUrl(QUrl(KAboutData::applicationData().homepage()));
}

void WelcomePageWidget::onLearnAboutTeamClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://userbase.kde.org/KDevelop")));
}

void WelcomePageWidget::onJoinTeamClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://kdevelop.org/contribute-kdevelop")));
}

void WelcomePageWidget::onHandbookClicked()
{
    triggerMainWindowActionById(QStringLiteral("help_contents"));
}

void WelcomePageWidget::onNewProjectClicked()
{
    triggerMainWindowActionById(QStringLiteral("project_new"));
}

void WelcomePageWidget::onOpenProjectClicked()
{
    triggerMainWindowActionById(QStringLiteral("project_open"));
}

void WelcomePageWidget::onFetchProjectClicked()
{
    triggerMainWindowActionById(QStringLiteral("project_fetch"));
}

void WelcomePageWidget::onRecentProjectsClicked()
{
    triggerMainWindowActionMenuById(QStringLiteral("project_open_recent"));
}

void WelcomePageWidget::onSessionEntered(const QModelIndex& index)
{
    if (index.isValid()) {
        m_ui->sessionsListView->setCursor(Qt::PointingHandCursor);
    }
}

void WelcomePageWidget::onSessionsViewportEntered()
{
    m_ui->sessionsListView->unsetCursor();
}

void WelcomePageWidget::onSessionClicked(const QModelIndex& index)
{
    const auto id = index.data(SessionListModel::SessionIdRole).toString();
    if (id.isEmpty()) {
        return;
    }
    KDevelop::Core::self()->sessionController()->loadSession(id);
}

void WelcomePageWidget::onSessionListSizeChanged(int size)
{
    // we always have at least one active session
    const bool showWelcomeText = (size < 2);
    m_ui->sessionsOrWelcomeWidget->setCurrentIndex(showWelcomeText ? 1 : 0);
}

#include "moc_welcomepageview.cpp"
