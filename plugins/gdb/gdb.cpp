/*
    SPDX-FileCopyrightText: 1999 John Birch <jbb@kdevelop.org >
    SPDX-FileCopyrightText: 2007 Vladimir Prus <ghost@cs.msu.su>
    SPDX-FileCopyrightText: 2016 Aetf <aetf@unlimitedcodeworks.xyz>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gdb.h"

#include "dbgglobal.h"
#include "debuglog.h"

#include <interfaces/icore.h>
#include <interfaces/iruntime.h>
#include <interfaces/iruntimecontroller.h>
#include <interfaces/iuicontroller.h>
#include <sublime/message.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KShell>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QUrl>

using namespace KDevMI::GDB;
using namespace KDevMI::MI;

namespace {
QString packagedRriseToolkitRoot()
{
#ifdef Q_OS_WIN
    QDir appDir(QApplication::applicationDirPath());
    if (appDir.dirName().compare(QLatin1String("bin"), Qt::CaseInsensitive) == 0) {
        appDir.cdUp();
    }

    const QString toolkitRoot = QDir::cleanPath(appDir.filePath(QStringLiteral("riscv_toolkit")));
    if (QFileInfo::exists(toolkitRoot)) {
        return toolkitRoot;
    }
#endif
    return {};
}

QString expandWindowsEnvironmentVariables(QString value)
{
#ifdef Q_OS_WIN
    const auto environment = QProcessEnvironment::systemEnvironment();
    qsizetype searchFrom = 0;
    while (true) {
        const qsizetype start = value.indexOf(QLatin1Char('%'), searchFrom);
        if (start < 0) {
            break;
        }

        const qsizetype end = value.indexOf(QLatin1Char('%'), start + 1);
        if (end < 0) {
            break;
        }

        const QString name = value.mid(start + 1, end - start - 1);
        QString replacement = environment.value(name);
        if (replacement.isEmpty() && name.compare(QLatin1String("RRISE_TOOLKIT_ROOT"), Qt::CaseInsensitive) == 0) {
            replacement = packagedRriseToolkitRoot();
        }
        if (replacement.isEmpty()) {
            searchFrom = end + 1;
            continue;
        }

        value.replace(start, end - start + 1, replacement);
        searchFrom = start + replacement.size();
    }
#endif
    return value;
}

QString resolveDebuggerExecutable(const QString& rawValue)
{
    if (rawValue.isEmpty()) {
        return {};
    }

    const QUrl url(rawValue);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }

    const QString expanded = expandWindowsEnvironmentVariables(rawValue);
    const QUrl expandedUrl(expanded);
    if (expandedUrl.isLocalFile()) {
        return expandedUrl.toLocalFile();
    }
    if (QDir::isAbsolutePath(expanded)) {
        return QDir::cleanPath(expanded);
    }

    return expanded;
}
}

GdbDebugger::GdbDebugger(QObject* parent)
    : MIDebugger(parent)
{
}

GdbDebugger::~GdbDebugger()
{
}

bool GdbDebugger::start(KConfigGroup& config, const QStringList& extraArguments)
{
    // FIXME: verify that default value leads to something sensible
    const QString configuredGdb = resolveDebuggerExecutable(config.readEntry(Config::GdbPathEntry, QString()));
    if (configuredGdb.isEmpty()) {
        m_debuggerExecutable = QStringLiteral("gdb");
    } else {
        m_debuggerExecutable = configuredGdb;
    }

    QStringList arguments = extraArguments;
    arguments << QStringLiteral("--interpreter=mi2") << QStringLiteral("-quiet");

    QString fullCommand;

    const QString shell = resolveDebuggerExecutable(config.readEntry(Config::DebuggerShellEntry, QString()).trimmed());
    if(!shell.isEmpty()) {
        qCDebug(DEBUGGERGDB) << "have shell" << shell;
        QString shell_without_args = shell.split(QLatin1Char(' ')).first();

        QFileInfo info(shell_without_args);
        /*if( info.isRelative() )
        {
            shell_without_args = build_dir + "/" + shell_without_args;
            info.setFile( shell_without_args );
        }*/
        if(!info.exists()) {
            const QString messageText = i18n("Could not locate the debugging shell '%1'.", shell_without_args);
            auto* message = new Sublime::Message(messageText, Sublime::Message::Error);
            KDevelop::ICore::self()->uiController()->postMessage(message);
            return false;
        }

        arguments.insert(0, m_debuggerExecutable);
        arguments.insert(0, shell);
        m_process->setShellCommand(KShell::joinArgs(arguments));
    } else {
        m_process->setProgram(m_debuggerExecutable, arguments);
        fullCommand = m_debuggerExecutable + QLatin1Char(' ');
    }
    fullCommand += arguments.join(QLatin1Char(' '));

    KDevelop::ICore::self()->runtimeController()->currentRuntime()->startProcess(m_process);

    qCDebug(DEBUGGERGDB) << "Starting GDB with command" << fullCommand;
    qCDebug(DEBUGGERGDB) << "GDB process pid:" << m_process->processId();
    emit userCommandOutput(fullCommand + QLatin1Char('\n'));
    return true;
}

#include "moc_gdb.cpp"
