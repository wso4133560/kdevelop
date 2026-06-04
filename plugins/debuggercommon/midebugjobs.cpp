/*
    SPDX-FileCopyrightText: 2006 Vladimir Prus <ghost@cs.msu.su>
    SPDX-FileCopyrightText: 2007 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2009 Andreas Pakulat <apaku@gmx.de>
    SPDX-FileCopyrightText: 2016 Aetf <aetf@unlimitedcodeworks.xyz>
    SPDX-FileCopyrightText: 2025 Igor Kushnir <igorkuo@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "midebugjobs.h"

#include "debuglog.h"
#include "dbgglobal.h"
#include "midebugsession.h"
#include "midebuggerplugin.h"

#include <execute/iexecuteplugin.h>
#include <interfaces/iproject.h>
#include <interfaces/ilaunchconfiguration.h>
#include <outputview/outputmodel.h>

#include <KConfigGroup>
#include <KLocalizedString>

#include <QFileInfo>
#include <QUrl>

using namespace KDevMI;
using namespace KDevelop;

namespace {
QString displayLaunchName(const QString& launchName)
{
    if (launchName == QLatin1String("CK803 ifftt remote")) {
        return QStringLiteral("TR201 remote");
    }
    return launchName;
}

bool usesRemoteGdbScripts(const KConfigGroup& grp)
{
    return grp.readEntry(GDB::Config::RemoteGdbConfigEntry, QUrl()).isValid()
        || grp.readEntry(GDB::Config::RemoteGdbShellEntry, QUrl()).isValid()
        || grp.readEntry(GDB::Config::RemoteGdbRunEntry, QUrl()).isValid();
}
}

template<class JobBase>
MIDebugJobBase<JobBase>::MIDebugJobBase(MIDebuggerPlugin* plugin, QObject* parent)
    : JobBase(parent)
    , m_plugin(plugin)
{
    Q_ASSERT(plugin);

    JobBase::setCapabilities(KJob::Killable);

    qCDebug(DEBUGGERCOMMON) << "created debug job" << this;
}

template<class JobBase>
MIDebugJobBase<JobBase>::~MIDebugJobBase()
{
    if (this->isFinished()) {
        // do not print m_session, because it can be already destroyed
        qCDebug(DEBUGGERCOMMON) << "destroying debug job" << this;
    } else {
        qCDebug(DEBUGGERCOMMON) << "destroying debug job" << this << "before it finished";
        if (m_session) {
            stopDebugger();
        }
    }
}

template<typename JobBase>
void MIDebugJobBase<JobBase>::done()
{
    qCDebug(DEBUGGERCOMMON) << "finishing debug job" << this << "with" << m_session;
    JobBase::emitResult();
}

template<typename JobBase>
MIDebugSession* MIDebugJobBase<JobBase>::createAndRegisterSession()
{
    auto* const session = m_plugin->createSession();
    trackSession(session);
    return session;
}

template<typename JobBase>
void MIDebugJobBase<JobBase>::trackSession(MIDebugSession* session)
{
    Q_ASSERT(session);
    Q_ASSERT(!m_session);

    m_session = session;
    QObject::connect(m_session, &MIDebugSession::stateChanged, this, [this](IDebugSession::DebuggerState state) {
        if (state == IDebugSession::EndedState) {
            done();
        }
    });
}

template<typename JobBase>
void MIDebugJobBase<JobBase>::stopDebugger()
{
    Q_ASSERT(m_session);
    qCDebug(DEBUGGERCOMMON) << "stopping debugger of" << m_session;
    QObject::disconnect(m_session, &MIDebugSession::stateChanged, this, nullptr);
    m_session->stopDebugger();
}

template<typename JobBase>
bool MIDebugJobBase<JobBase>::doKill()
{
    qCDebug(DEBUGGERCOMMON) << "killing debug job" << this;
    if (m_session) {
        stopDebugger();
    }
    return true;
}

MIDebugJob::MIDebugJob(MIDebuggerPlugin* p, ILaunchConfiguration* launchcfg,
                   IExecutePlugin* execute, QObject* parent)
    : MIDebugJobBase(p, parent)
{
    Q_ASSERT(launchcfg);
    Q_ASSERT(execute);

    initializeStartupInfo(execute, launchcfg);
    if (!m_startupInfo) {
        qCDebug(DEBUGGERCOMMON) << "failing debug job" << this;
        // The dependency job, if any, will not be created, and an output view
        // for this job will never be added, so do not raise any tool view.
        return;
    }

    const auto launchName = displayLaunchName(launchcfg->name());
    if (launchcfg->project()) {
        setObjectName(i18nc("ProjectName: run configuration name", "%1: %2",
                            launchcfg->project()->name(), launchName));
    } else {
        setObjectName(launchName);
    }
}

void MIDebugJob::start()
{
    if (!m_startupInfo) {
        Q_ASSERT(error() != NoError);
        emitResult();
        return;
    }
    Q_ASSERT(error() == NoError);

    if (!m_plugin->prepareDebugging(*m_startupInfo)) {
        m_startupInfo.reset();
        done();
        return;
    }
    createAndRegisterSession();

    // This job may have a dependency builder job. If the dependency job fails after
    // the debug session is registered, raise the Build output tool view to show build error(s).
    m_session->setToolViewToRaiseAtEnd(IDebugSession::ToolView::Build);

    setStandardToolView(IOutputView::DebugView);
    setBehaviours(IOutputView::Behaviours(IOutputView::AllowUserClose) | KDevelop::IOutputView::AutoScroll);

    auto model = new KDevelop::OutputModel;
    model->setFilteringStrategy(OutputModel::NativeAppErrorFilter);
    setModel(model);

    connect(m_session, &MIDebugSession::inferiorStdoutLines, model, &OutputModel::appendLines);
    connect(m_session, &MIDebugSession::inferiorStderrLines, model, &OutputModel::appendLines);

    setTitle(displayLaunchName(m_startupInfo->launchConfiguration->name()));

    const auto grp = m_startupInfo->launchConfiguration->config();
    QString startWith = grp.readEntry(Config::StartWithEntry, QStringLiteral("ApplicationOutput"));
    if (startWith == QLatin1String("ApplicationOutput")) {
        setVerbosity(Verbose);
    } else {
        setVerbosity(Silent);
    }

    startOutput();

    // An output view for this job was just added to the Debug tool view. Raise the
    // Debug tool view in the end to show the output of the debuggee in the Code area.
    m_session->setToolViewToRaiseAtEnd(IDebugSession::ToolView::Debug);

    if (!m_session->startDebugging(std::move(*m_startupInfo))) {
        done();
    }
    m_startupInfo.reset(); // no more use for the info, so release the memory
}

void MIDebugJob::initializeStartupInfo(IExecutePlugin* execute, ILaunchConfiguration* launchConfiguration)
{
    QString errorString;
    const auto detectError = [&errorString, this](int errorCode) {
        if (errorString.isEmpty()) {
            return false;
        }
        setError(errorCode);
        setErrorText(errorString);
        return true;
    };

    auto executable = execute->executable(launchConfiguration, errorString).toLocalFile();
    if (detectError(InvalidExecutable)) {
        return;
    }
    const QFileInfo executableInfo{executable};
    if (!executableInfo.exists()) {
        setError(InvalidExecutable);
        setErrorText(i18n("'%1' does not exist", executable));
        return;
    }
    if (!executableInfo.isExecutable() && !usesRemoteGdbScripts(launchConfiguration->config())) {
        setError(ExecutableIsNotExecutable);
        setErrorText(i18n("'%1' is not an executable", executable));
        return;
    }

    auto arguments = execute->arguments(launchConfiguration, errorString);
    if (detectError(InvalidArguments)) {
        return;
    }

    QStringList terminal;
    if (execute->useTerminal(launchConfiguration)) {
        terminal = execute->terminal(launchConfiguration, errorString);
        if (detectError(InvalidExternalTerminal)) {
            return;
        }
    }

    m_startupInfo.reset(new InferiorStartupInfo{execute, launchConfiguration, std::move(executable),
                                                std::move(arguments), std::move(terminal)});
}

MIExamineCoreJob::MIExamineCoreJob(MIDebuggerPlugin* plugin, CoreInfo coreInfo, QObject* parent)
    : MIDebugJobBase(plugin, parent)
    , m_coreInfo{std::move(coreInfo)}
{
    // This job has no dependency job and does not show output, so do not raise any tool view.
    setObjectName(i18n("Debug core file"));
}

void MIExamineCoreJob::start()
{
    createAndRegisterSession();
    if (!m_session->examineCoreFile(std::move(m_coreInfo.executableFile), std::move(m_coreInfo.coreFile))) {
        done();
    }
    m_coreInfo = {}; // no more use for the info, so release the memory
}

MIAttachProcessJob::MIAttachProcessJob(MIDebuggerPlugin *plugin, int pid, QObject *parent)
    : MIDebugJobBase(plugin, parent)
    , m_pid(pid)
{
    // This job has no dependency job and does not show output, so do not raise any tool view.
    setObjectName(i18n("Debug process %1", pid));
}

void MIAttachProcessJob::start()
{
    createAndRegisterSession();
    if (!m_session->attachToProcess(m_pid)) {
        done();
    }
}

#include "moc_midebugjobs.cpp"
