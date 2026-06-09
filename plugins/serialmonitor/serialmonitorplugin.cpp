/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <interfaces/icore.h>
#include <interfaces/idebugcontroller.h>
#include <interfaces/iplugin.h>
#include <interfaces/iuicontroller.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMutex>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

#include <qt_windows.h>
#include <setupapi.h>

class SerialMonitorPlugin;

struct SerialPortInfo
{
    QString portName;
    QString displayName;
};

static constexpr GUID PortsClassGuid = {
    0x4d36e978,
    0xe325,
    0x11ce,
    {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18},
};

class SerialReader : public QThread
{
    Q_OBJECT

public:
    SerialReader(QString portName, int baudRate, QObject* parent = nullptr)
        : QThread(parent)
        , m_portName(std::move(portName))
        , m_baudRate(baudRate)
    {
    }

    void stop()
    {
        requestInterruption();
        QMutexLocker locker(&m_handleMutex);
        if (m_handle != INVALID_HANDLE_VALUE) {
            CancelIoEx(m_handle, nullptr);
        }
    }

Q_SIGNALS:
    void opened(const QString& portName, int baudRate);
    void bytesReady(const QByteArray& data);
    void failed(const QString& message);
    void closed(const QString& reason);

protected:
    void run() override
    {
        const QString deviceName = m_portName.startsWith(QStringLiteral("\\\\.\\"))
            ? m_portName
            : QStringLiteral("\\\\.\\") + m_portName;

        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(deviceName.utf16()),
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            emit failed(i18n("Failed to open %1. Windows error: %2", m_portName, GetLastError()));
            emit closed(i18n("open failed"));
            return;
        }

        {
            QMutexLocker locker(&m_handleMutex);
            m_handle = handle;
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (!GetCommState(handle, &dcb)) {
            emit failed(i18n("Failed to read serial port settings for %1. Windows error: %2", m_portName, GetLastError()));
            closeHandle();
            emit closed(i18n("GetCommState failed"));
            return;
        }

        dcb.BaudRate = static_cast<DWORD>(m_baudRate);
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;

        if (!SetCommState(handle, &dcb)) {
            emit failed(i18n("Failed to configure %1. Windows error: %2", m_portName, GetLastError()));
            closeHandle();
            emit closed(i18n("SetCommState failed"));
            return;
        }

        COMMTIMEOUTS timeouts = {};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        SetCommTimeouts(handle, &timeouts);
        PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

        emit opened(m_portName, m_baudRate);

        QByteArray buffer;
        buffer.resize(1024);
        QString closeReason = i18n("reader stopped");
        while (!isInterruptionRequested()) {
            DWORD bytesRead = 0;
            const BOOL ok = ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
            if (!ok) {
                const DWORD error = GetLastError();
                closeReason = i18n("ReadFile failed with Windows error %1", error);
                if (error != ERROR_OPERATION_ABORTED && !isInterruptionRequested()) {
                    emit failed(i18n("Serial read failed. Windows error: %1", error));
                } else if (isInterruptionRequested()) {
                    closeReason = i18n("requested close during read");
                } else {
                    closeReason = i18n("read operation aborted");
                }
                break;
            }
            if (bytesRead > 0) {
                emit bytesReady(QByteArray(buffer.constData(), static_cast<int>(bytesRead)));
            }
        }
        if (isInterruptionRequested()) {
            closeReason = i18n("requested close");
        }

        closeHandle();
        emit closed(closeReason);
    }

private:
    void closeHandle()
    {
        QMutexLocker locker(&m_handleMutex);
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    const QString m_portName;
    const int m_baudRate;
    QMutex m_handleMutex;
    HANDLE m_handle = INVALID_HANDLE_VALUE;
};

class SerialMonitorView : public QWidget
{
    Q_OBJECT

public:
    explicit SerialMonitorView(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_portCombo(new QComboBox(this))
        , m_baudCombo(new QComboBox(this))
        , m_refreshButton(new QPushButton(i18nc("@action:button", "Refresh"), this))
        , m_openButton(new QPushButton(i18nc("@action:button", "Open"), this))
        , m_closeButton(new QPushButton(i18nc("@action:button", "Close"), this))
        , m_clearButton(new QPushButton(i18nc("@action:button", "Clear"), this))
        , m_saveLogButton(new QPushButton(i18nc("@action:button", "Save Log..."), this))
        , m_statusLabel(new QLabel(this))
        , m_output(new QPlainTextEdit(this))
    {
        setObjectName(QStringLiteral("serialMonitorView"));
        applyThemeStyleSheet();

        m_portCombo->setEditable(true);
        m_baudCombo->setEditable(true);
        for (const int baud : {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600}) {
            m_baudCombo->addItem(QString::number(baud), baud);
        }

        m_output->setReadOnly(true);
        m_output->setMaximumBlockCount(5000);
        m_output->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto* controls = new QHBoxLayout;
        controls->addWidget(new QLabel(i18nc("@label:listbox", "Port:"), this));
        controls->addWidget(m_portCombo, 1);
        controls->addWidget(m_refreshButton);
        controls->addWidget(new QLabel(i18nc("@label:listbox", "Baud:"), this));
        controls->addWidget(m_baudCombo);
        controls->addWidget(m_openButton);
        controls->addWidget(m_closeButton);
        controls->addWidget(m_clearButton);
        controls->addWidget(m_saveLogButton);
        controls->addWidget(m_statusLabel, 1);

        auto* layout = new QVBoxLayout(this);
        layout->addLayout(controls);
        layout->addWidget(m_output, 1);

        connect(m_refreshButton, &QPushButton::clicked, this, &SerialMonitorView::refreshPorts);
        connect(m_openButton, &QPushButton::clicked, this, &SerialMonitorView::openPort);
        connect(m_closeButton, &QPushButton::clicked, this, &SerialMonitorView::closePort);
        connect(m_clearButton, &QPushButton::clicked, this, &SerialMonitorView::clearOutput);
        connect(m_saveLogButton, &QPushButton::clicked, this, &SerialMonitorView::chooseLogDirectory);

        s_views.push_back(this);
        readSettings();
        refreshPorts();
        syncFromSharedState();
    }

    ~SerialMonitorView() override
    {
        writeSettings();
        s_views.removeAll(this);
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange) {
            scheduleThemeStyleSheetUpdate();
        }
    }

public:
    static void autoOpenSavedPort()
    {
        if (s_portOpen || s_opening) {
            appendLog(i18n("[%1] Auto-open skipped: serial port is already open.\n",
                           QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
            return;
        }

        appendLog(i18n("[%1] Auto-opening saved serial port for debug session.\n",
                       QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
        if (s_views.isEmpty()) {
            appendLog(i18n("[%1] ERROR: Serial Port view is not ready; auto-open skipped.\n",
                           QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
            return;
        }

        s_views.constFirst()->openPort();
    }

private Q_SLOTS:
    void refreshPorts()
    {
        const QString previousPort = currentPortName();
        m_portCombo->clear();

        const QList<SerialPortInfo> ports = availablePorts();
        for (const SerialPortInfo& port : ports) {
            m_portCombo->addItem(port.displayName, port.portName);
        }

        if (!previousPort.isEmpty()) {
            const int index = findPortIndex(previousPort);
            if (index >= 0) {
                m_portCombo->setCurrentIndex(index);
            } else {
                m_portCombo->setEditText(previousPort);
            }
        } else if (m_portCombo->count() > 0) {
            m_portCombo->setCurrentIndex(0);
        }

        setStatus(i18np("Found %1 port.", "Found %1 ports.", ports.size()));
    }

    void openPort()
    {
        if (s_reader) {
            if (s_portOpen || s_opening) {
                setStatus(i18n("Open: serial port is already open."));
                return;
            }

            appendLog(i18n("[%1] Resetting stale serial reader before opening. open=%2 opening=%3\n",
                           QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                           s_portOpen ? QStringLiteral("true") : QStringLiteral("false"),
                           s_opening ? QStringLiteral("true") : QStringLiteral("false")));
            s_reader->disconnect();
            s_reader->stop();
            s_reader->wait(500);
            s_reader->deleteLater();
            s_reader = nullptr;
        }

        const QString portName = currentPortName();
        if (portName.isEmpty()) {
            setStatus(i18n("No serial port selected."));
            return;
        }

        bool ok = false;
        const int baudRate = m_baudCombo->currentText().trimmed().toInt(&ok);
        if (!ok || baudRate <= 0) {
            setStatus(i18n("Invalid baud rate."));
            return;
        }

        writeSettings();
        appendLog(i18n("[%1] Opening %2 @ %3\n",
                       QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                       portName,
                       QString::number(baudRate)));
        s_opening = true;
        s_portOpen = false;
        setStatus(i18n("Opening: %1 @ %2", portName, baudRate));
        updateViewsOpenState(true);

        s_reader = new SerialReader(portName, baudRate);
        s_totalBytes = 0;
        connect(s_reader, &SerialReader::opened, qApp, [](const QString& port, int baud) {
            s_opening = false;
            s_portOpen = true;
            appendLog(i18n("[%1] Opened %2 @ %3\n",
                           QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                           port,
                           QString::number(baud)));
            setStatus(i18n("Open: %1 @ %2, RX %3 bytes", port, QString::number(baud), QString::number(s_totalBytes)));
            updateViewsOpenState(true);
        });
        connect(s_reader, &SerialReader::bytesReady, qApp, [](const QByteArray& data) {
            s_totalBytes += static_cast<quint64>(data.size());
            appendBytes(data);
            setStatus(i18n("Open: RX %1 bytes", QString::number(s_totalBytes)));
        });
        connect(s_reader, &SerialReader::failed, qApp, [](const QString& message) {
            s_opening = false;
            s_portOpen = false;
            appendLog(i18n("[%1] ERROR: %2\n", QDateTime::currentDateTime().toString(Qt::ISODateWithMs), message));
            setStatus(message);
            updateViewsOpenState(false);
        });
        connect(s_reader, &SerialReader::closed, qApp, [](const QString& reason) {
            s_opening = false;
            s_portOpen = false;
            appendLog(i18n("[%1] Closed after RX %2 bytes: %3\n",
                           QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                           QString::number(s_totalBytes),
                           reason));
            setStatus(i18n("Closed"));
            updateViewsOpenState(false);
        });
        connect(s_reader, &QThread::finished, s_reader, &QObject::deleteLater);
        connect(s_reader, &QObject::destroyed, qApp, []() {
            s_reader = nullptr;
            s_opening = false;
            s_portOpen = false;
            updateViewsOpenState(false);
        });

        s_reader->start();
    }

    void closePort()
    {
        if (!s_reader) {
            return;
        }

        s_opening = false;
        s_reader->stop();
        if (!s_reader->wait(1500)) {
            appendLog(i18n("[%1] WARNING: serial reader did not stop within timeout.\n",
                           QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
        }
    }

    void chooseLogDirectory()
    {
        QString initialDirectory = s_logDirectory;
        if (initialDirectory.isEmpty()) {
            initialDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        }
        if (initialDirectory.isEmpty()) {
            initialDirectory = QDir::homePath();
        }

        const QString directory = QFileDialog::getExistingDirectory(this,
                                                                    i18nc("@title:window", "Select Serial Log Folder"),
                                                                    initialDirectory);
        if (directory.isEmpty()) {
            return;
        }

        startSavingLog(directory);
    }

    void clearOutput()
    {
        s_output.clear();
        for (SerialMonitorView* view : std::as_const(s_views)) {
            view->m_output->clear();
        }
    }

private:
    void applyThemeStyleSheet()
    {
        if (m_themeStyleSheetApplying) {
            return;
        }

        m_themeStyleSheetApplying = true;
        const QPalette palette = QApplication::palette();
        const bool dark = palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();
        if (!dark) {
            setStyleSheet(QString{});
            m_themeStyleSheetApplying = false;
            return;
        }

        setStyleSheet(QStringLiteral(
            "#serialMonitorView {"
            "background: #1e1e1e;"
            "color: #d4d4d4;"
            "}"
            "#serialMonitorView QLabel {"
            "color: #d4d4d4;"
            "}"
            "#serialMonitorView QComboBox {"
            "background: #2d2d30;"
            "color: #d4d4d4;"
            "selection-background-color: #264f78;"
            "selection-color: #ffffff;"
            "border: 1px solid #555555;"
            "border-radius: 3px;"
            "padding: 3px 8px;"
            "min-height: 22px;"
            "}"
            "#serialMonitorView QComboBox:disabled {"
            "background: #252526;"
            "color: #858585;"
            "border-color: #3c3c3c;"
            "}"
            "#serialMonitorView QComboBox QAbstractItemView {"
            "background: #252526;"
            "color: #d4d4d4;"
            "selection-background-color: #264f78;"
            "selection-color: #ffffff;"
            "border: 1px solid #3c3c3c;"
            "}"
            "#serialMonitorView QLineEdit {"
            "background: #2d2d30;"
            "color: #d4d4d4;"
            "selection-background-color: #264f78;"
            "selection-color: #ffffff;"
            "border: 0;"
            "}"
            "#serialMonitorView QPushButton {"
            "background: #3c3c3c;"
            "color: #d4d4d4;"
            "border: 1px solid #555555;"
            "border-radius: 3px;"
            "padding: 4px 12px;"
            "min-height: 22px;"
            "}"
            "#serialMonitorView QPushButton:hover {"
            "background: #4b4b4b;"
            "border-color: #6b6b6b;"
            "}"
            "#serialMonitorView QPushButton:pressed {"
            "background: #264f78;"
            "color: #ffffff;"
            "}"
            "#serialMonitorView QPushButton:disabled {"
            "background: #2d2d30;"
            "color: #6a6a6a;"
            "border-color: #3c3c3c;"
            "}"
            "#serialMonitorView QPlainTextEdit {"
            "background: #111315;"
            "color: #d4d4d4;"
            "selection-background-color: #264f78;"
            "selection-color: #ffffff;"
            "border: 1px solid #3c3c3c;"
            "border-radius: 3px;"
            "}"));
        m_themeStyleSheetApplying = false;
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

    static QList<SerialPortInfo> availablePorts()
    {
        QList<SerialPortInfo> ports = setupApiPorts();
        QSet<QString> knownPorts;
        for (const SerialPortInfo& port : ports) {
            knownPorts.insert(port.portName.toUpper());
        }

        wchar_t target[512] = {};
        for (int i = 1; i <= 256; ++i) {
            const QString port = QStringLiteral("COM%1").arg(i);
            if (!knownPorts.contains(port.toUpper())
                && QueryDosDeviceW(reinterpret_cast<LPCWSTR>(port.utf16()), target, std::size(target)) != 0) {
                ports.push_back({port, port});
            }
        }
        return ports;
    }

    static QList<SerialPortInfo> setupApiPorts()
    {
        QList<SerialPortInfo> ports;
        HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&PortsClassGuid, nullptr, nullptr, DIGCF_PRESENT);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            return ports;
        }

        SP_DEVINFO_DATA deviceInfo = {};
        deviceInfo.cbSize = sizeof(SP_DEVINFO_DATA);
        for (DWORD index = 0; SetupDiEnumDeviceInfo(deviceInfoSet, index, &deviceInfo); ++index) {
            const QString portName = registryPortName(deviceInfoSet, deviceInfo);
            if (portName.isEmpty() || !portName.startsWith(QStringLiteral("COM"), Qt::CaseInsensitive)) {
                continue;
            }

            QString displayName = registryProperty(deviceInfoSet, deviceInfo, SPDRP_FRIENDLYNAME);
            if (displayName.isEmpty()) {
                displayName = registryProperty(deviceInfoSet, deviceInfo, SPDRP_DEVICEDESC);
            }
            if (displayName.isEmpty()) {
                displayName = portName;
            } else if (!displayName.contains(portName, Qt::CaseInsensitive)) {
                displayName = i18n("%1 (%2)", displayName, portName);
            }

            ports.push_back({portName.toUpper(), displayName});
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return ports;
    }

    static QString registryProperty(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfo, DWORD property)
    {
        wchar_t buffer[1024] = {};
        DWORD propertyType = 0;
        DWORD requiredSize = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet,
                                               &deviceInfo,
                                               property,
                                               &propertyType,
                                               reinterpret_cast<PBYTE>(buffer),
                                               sizeof(buffer),
                                               &requiredSize)) {
            return {};
        }
        return QString::fromWCharArray(buffer).trimmed();
    }

    static QString registryPortName(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfo)
    {
        HKEY key = SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (key == INVALID_HANDLE_VALUE) {
            return {};
        }

        wchar_t buffer[256] = {};
        DWORD type = 0;
        DWORD size = sizeof(buffer);
        const LONG result = RegQueryValueExW(key,
                                             L"PortName",
                                             nullptr,
                                             &type,
                                             reinterpret_cast<LPBYTE>(buffer),
                                             &size);
        RegCloseKey(key);

        if (result != ERROR_SUCCESS || type != REG_SZ) {
            return {};
        }
        return QString::fromWCharArray(buffer).trimmed();
    }

    static QString portNameFromText(const QString& text)
    {
        static const QRegularExpression portRegExp(QStringLiteral("\\b(COM\\d+)\\b"),
                                                   QRegularExpression::CaseInsensitiveOption);
        const auto match = portRegExp.match(text);
        if (match.hasMatch()) {
            return match.captured(1).toUpper();
        }
        return text.trimmed();
    }

    QString currentPortName() const
    {
        const QString text = m_portCombo->currentText().trimmed();
        const int index = m_portCombo->currentIndex();
        if (index >= 0 && m_portCombo->itemText(index) == text) {
            const QString portName = m_portCombo->itemData(index).toString();
            if (!portName.isEmpty()) {
                return portName;
            }
        }
        return portNameFromText(text);
    }

    int findPortIndex(const QString& portName) const
    {
        for (int i = 0; i < m_portCombo->count(); ++i) {
            if (m_portCombo->itemData(i).toString().compare(portName, Qt::CaseInsensitive) == 0) {
                return i;
            }
        }
        return -1;
    }

    static void appendBytes(const QByteArray& data)
    {
        appendText(QString::fromLocal8Bit(data));
    }

    static void appendLog(const QString& message)
    {
        appendText(message);
    }

    static void appendText(const QString& text)
    {
        s_output += text;
        if (s_output.size() > 200000) {
            s_output.remove(0, s_output.size() - 200000);
        }

        writeLogFile(text);

        for (SerialMonitorView* view : std::as_const(s_views)) {
            view->m_output->moveCursor(QTextCursor::End);
            view->m_output->insertPlainText(text);
            view->m_output->moveCursor(QTextCursor::End);
        }
    }

    static void setStatus(const QString& status)
    {
        s_status = status;
        for (SerialMonitorView* view : std::as_const(s_views)) {
            view->m_statusLabel->setText(status);
        }
    }

    static void updateViewsOpenState(bool open)
    {
        for (SerialMonitorView* view : std::as_const(s_views)) {
            view->updateUi(open);
        }
    }

    static void writeLogFile(const QString& text)
    {
        if (!s_logFile || !s_logFile->isOpen()) {
            return;
        }

        s_logFile->write(text.toUtf8());
        s_logFile->flush();
    }

    static void updateViewsLogState()
    {
        for (SerialMonitorView* view : std::as_const(s_views)) {
            view->updateLogUi();
        }
    }

    void startSavingLog(const QString& directory)
    {
        QDir dir(directory);
        if (!dir.exists()) {
            setStatus(i18n("Log folder does not exist: %1", directory));
            return;
        }

        const QString fileName = QStringLiteral("serial-port-%1.log")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        const QString filePath = dir.filePath(fileName);

        auto logFile = std::make_unique<QFile>(filePath);
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
            setStatus(i18n("Failed to open log file: %1", logFile->errorString()));
            return;
        }

        if (!s_output.isEmpty()) {
            logFile->write(s_output.toUtf8());
            logFile->flush();
        }

        s_logDirectory = dir.absolutePath();
        s_logFilePath = filePath;
        s_logFile = std::move(logFile);

        KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SerialMonitor"));
        config.writeEntry("LogDirectory", s_logDirectory);
        config.sync();

        appendLog(i18n("[%1] Saving serial log to %2\n",
                       QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                       QDir::toNativeSeparators(s_logFilePath)));
        setStatus(i18n("Saving log: %1", QDir::toNativeSeparators(s_logFilePath)));
        updateViewsLogState();
    }

    void syncFromSharedState()
    {
        m_output->setPlainText(s_output);
        m_output->moveCursor(QTextCursor::End);
        m_statusLabel->setText(s_status);
        updateUi(s_portOpen || s_opening);
        updateLogUi();
    }

    void updateUi(bool open)
    {
        m_portCombo->setEnabled(!open);
        m_baudCombo->setEnabled(!open);
        m_refreshButton->setEnabled(!open);
        m_openButton->setEnabled(!open);
        m_closeButton->setEnabled(open);
    }

    void updateLogUi()
    {
        if (s_logFile && s_logFile->isOpen()) {
            m_saveLogButton->setText(i18nc("@action:button", "Change Log Folder..."));
            m_saveLogButton->setToolTip(i18n("Serial output is being saved to: %1",
                                             QDir::toNativeSeparators(s_logFilePath)));
        } else {
            m_saveLogButton->setText(i18nc("@action:button", "Save Log..."));
            m_saveLogButton->setToolTip(i18n("Select a folder and save current and future serial output to a log file."));
        }
    }

    void readSettings()
    {
        const KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SerialMonitor"));
        const QString port = config.readEntry("Port", QString());
        const int baud = config.readEntry("BaudRate", 115200);
        if (s_logDirectory.isEmpty()) {
            s_logDirectory = config.readEntry("LogDirectory", QString());
        }
        if (!port.isEmpty()) {
            m_portCombo->setEditText(port);
        }
        const int baudIndex = m_baudCombo->findText(QString::number(baud));
        if (baudIndex >= 0) {
            m_baudCombo->setCurrentIndex(baudIndex);
        } else {
            m_baudCombo->setEditText(QString::number(baud));
        }
    }

    void writeSettings() const
    {
        KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("SerialMonitor"));
        config.writeEntry("Port", currentPortName());
        config.writeEntry("BaudRate", m_baudCombo->currentText().trimmed().toInt());
        config.sync();
    }

    QComboBox* const m_portCombo;
    QComboBox* const m_baudCombo;
    QPushButton* const m_refreshButton;
    QPushButton* const m_openButton;
    QPushButton* const m_closeButton;
    QPushButton* const m_clearButton;
    QPushButton* const m_saveLogButton;
    QLabel* const m_statusLabel;
    QPlainTextEdit* const m_output;
    bool m_themeStyleSheetUpdatePending = false;
    bool m_themeStyleSheetApplying = false;

    static QList<SerialMonitorView*> s_views;
    static SerialReader* s_reader;
    static bool s_opening;
    static bool s_portOpen;
    static quint64 s_totalBytes;
    static QString s_output;
    static QString s_status;
    static QString s_logDirectory;
    static QString s_logFilePath;
    static std::unique_ptr<QFile> s_logFile;
};

QList<SerialMonitorView*> SerialMonitorView::s_views;
SerialReader* SerialMonitorView::s_reader = nullptr;
bool SerialMonitorView::s_opening = false;
bool SerialMonitorView::s_portOpen = false;
quint64 SerialMonitorView::s_totalBytes = 0;
QString SerialMonitorView::s_output;
QString SerialMonitorView::s_status;
QString SerialMonitorView::s_logDirectory;
QString SerialMonitorView::s_logFilePath;
std::unique_ptr<QFile> SerialMonitorView::s_logFile;

class SerialMonitorPlugin : public KDevelop::IPlugin
{
    Q_OBJECT

public:
    SerialMonitorPlugin(QObject* parent, const KPluginMetaData& metaData, const QVariantList&)
        : KDevelop::IPlugin(QStringLiteral("kdevserialmonitor"), parent, metaData)
        , m_factory(new SerialToolViewFactory)
    {
        const QString toolViewName = i18nc("@title:window", "Serial Port");
        core()->uiController()->addToolView(toolViewName, m_factory, KDevelop::IUiController::CreateAndRaise);
        auto raiseSerialView = [this, toolViewName]() {
            core()->uiController()->findToolView(toolViewName, m_factory, KDevelop::IUiController::CreateAndRaise);
        };
        QTimer::singleShot(0, this, raiseSerialView);
        connect(core()->debugController(),
                &KDevelop::IDebugController::currentSessionChanged,
                this,
                [this, raiseSerialView](KDevelop::IDebugSession* session, KDevelop::IDebugSession*) {
            QTimer::singleShot(0, this, raiseSerialView);
            QTimer::singleShot(500, this, raiseSerialView);
            Q_UNUSED(session);
        });
    }

    void unload() override
    {
        core()->uiController()->removeToolView(m_factory);
    }

private:
    class SerialToolViewFactory : public KDevelop::IToolViewFactory
    {
    public:
        QWidget* create(QWidget* parent = nullptr) override
        {
            return new SerialMonitorView(parent);
        }

        Qt::DockWidgetArea defaultPosition() const override
        {
            return Qt::BottomDockWidgetArea;
        }

        QString id() const override
        {
            return QStringLiteral("org.kdevelop.SerialMonitor");
        }
    };

    SerialToolViewFactory* const m_factory;
};

K_PLUGIN_FACTORY_WITH_JSON(SerialMonitorFactory, "kdevserialmonitor.json", registerPlugin<SerialMonitorPlugin>();)

#include "serialmonitorplugin.moc"
