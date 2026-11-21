// no-port-check: NereusSDR-original station-helper options for Setup > Options.
#include "StationHelperOptions.h"
#include "gui/containers/ContainerManager.h"
#include "core/AppSettings.h"
#include "gui/HelperApps.h"
#include "gui/ScriptRunner.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {
namespace {

QPushButton* smallButton(const QString& text, QWidget* parent)
{
    auto* value = new QPushButton(text, parent);
    value->setMinimumHeight(24);
    return value;
}

} // namespace

StationHelperOptions::StationHelperOptions(RadioModel* model, QWidget* parent)
    : QWidget(parent)
{
    Q_UNUSED(model)
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    AppSettings& settings = AppSettings::instance();
    HelperApps& helpers = HelperApps::instance();

    // ---- Station helpers -------------------------------------------------
    auto* helperGroup = new QGroupBox(tr("Station helpers"), this);
    auto* helperLayout = new QVBoxLayout(helperGroup);
    root->addWidget(helperGroup);

    auto* helperInfo = new QLabel(tr("Optional. The helper's files are copied to your user "
                                     "folder only when it is enabled."), helperGroup);
    helperInfo->setWordWrap(true);
    helperLayout->addWidget(helperInfo);

    auto* helperForm = new QFormLayout;
    helperLayout->addLayout(helperForm);

    auto* waveNodeInfo = new QLabel(tr("WaveNode: enabled from Containers → Helpers."), helperGroup);
    waveNodeInfo->setWordWrap(true);
    helperForm->addRow(waveNodeInfo);
    m_waveStatus = new QLabel(helperGroup);
    m_waveStatus->setWordWrap(true);
    helperForm->addRow(tr("Status:"), m_waveStatus);
    auto* removeWave = smallButton(tr("Remove installed files"), helperGroup);
    helperForm->addRow(removeWave);

    auto* hfAutoInfo = new QLabel(tr("HF-Auto: enabled from Containers → Helpers."), helperGroup);
    hfAutoInfo->setWordWrap(true);
    helperForm->addRow(hfAutoInfo);
    m_hfFollow = new QCheckBox(tr("Enable frequency tracking (Tuner Follow)"), helperGroup);
    m_hfFollow->setToolTip(tr("Independent of the Containers → Helpers switch."));
    helperForm->addRow(m_hfFollow);
    m_hfStatus = new QLabel(helperGroup);
    m_hfStatus->setWordWrap(true);
    helperForm->addRow(tr("Status:"), m_hfStatus);
    auto* removeHf = smallButton(tr("Remove installed files"), helperGroup);
    helperForm->addRow(removeHf);

    auto* resetContainers = smallButton(tr("Reset helper containers"), helperGroup);
    helperForm->addRow(resetContainers);
    connect(resetContainers, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Reset helper containers"),
                                  tr("Remove the WaveNode/HF-Auto containers, including an old "
                                     "combined one, and import the Thetis layouts again?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        if (auto* manager = qApp->findChild<ContainerManager*>()) {
            manager->resetHelperContainers();
        }
        updateHelperStatus();
    });

    m_syncing = true;
    m_hfFollow->setChecked(settings.value(QStringLiteral("Helpers/HfAutoFollow"), true).toBool());
    m_syncing = false;
    m_hfFollow->setEnabled(helpers.enabled(HelperApps::Helper::HfAuto));

    connect(m_hfFollow, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("Helpers/HfAutoFollow"), on);
    });
    connect(&helpers, &HelperApps::enabledChanged, this,
            [this](HelperApps::Helper helper, bool) {
                if (helper == HelperApps::Helper::HfAuto && m_hfFollow) {
                    m_hfFollow->setEnabled(HelperApps::instance().enabled(helper));
                }
                updateHelperStatus();
            });
    connect(&helpers, &HelperApps::runningChanged, this,
            [this](HelperApps::Helper, bool) { updateHelperStatus(); });
    connect(&helpers, &HelperApps::fault, this,
            [this](HelperApps::Helper helper, const QString& reason) {
                if (helper == HelperApps::Helper::WaveNode) { m_waveFault = reason; }
                else { m_hfFault = reason; }
                updateHelperStatus();
            });

    const auto removeHelper = [this](HelperApps::Helper helper, const QString& title) {
        if (QMessageBox::question(this, tr("Remove %1").arg(title),
                                  tr("Remove the installed %1 files from your user folder? "
                                     "They are copied again if you enable it later.").arg(title),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        HelperApps& current = HelperApps::instance();
        current.stop(helper);
        current.setEnabled(helper, false);
        QString error;
        if (!HelperApps::removeInstalled(helper, &error)) {
            QMessageBox::warning(this, tr("Remove %1").arg(title), error);
        }
        updateHelperStatus();
    };
    connect(removeWave, &QPushButton::clicked, this,
            [this, removeHelper] { removeHelper(HelperApps::Helper::WaveNode, tr("WaveNode")); });
    connect(removeHf, &QPushButton::clicked, this,
            [this, removeHelper] { removeHelper(HelperApps::Helper::HfAuto, tr("HF-Auto")); });

    // ---- Start and exit scripts -----------------------------------------
    auto* scriptGroup = new QGroupBox(tr("Start and exit scripts"), this);
    auto* scriptLayout = new QVBoxLayout(scriptGroup);
    root->addWidget(scriptGroup);

    auto* scriptInfo = new QLabel(tr("Optional. Run a batch file when NereusSDR starts and when it "
                                     "exits, for example the Denkovi relay that powers the SDR. The "
                                     "scripts are copied to your user folder only when enabled."), scriptGroup);
    scriptInfo->setWordWrap(true);
    scriptLayout->addWidget(scriptInfo);

    const auto addScriptRow = [this, scriptLayout](const QString& label, QCheckBox** check,
                                                   QLineEdit** edit, const QString& placeholder) {
        auto* row = new QHBoxLayout;
        *check = new QCheckBox(label, this);
        row->addWidget(*check);
        *edit = new QLineEdit(this);
        (*edit)->setPlaceholderText(placeholder);
        row->addWidget(*edit, 1);
        auto* browseButton = smallButton(tr("Browse\u2026"), this);
        auto* editButton = smallButton(tr("Edit"), this);
        auto* folderButton = smallButton(tr("Folder"), this);
        row->addWidget(browseButton);
        row->addWidget(editButton);
        row->addWidget(folderButton);
        scriptLayout->addLayout(row);
        connect(browseButton, &QPushButton::clicked, this, [this, edit] { browse(*edit); });
        connect(editButton, &QPushButton::clicked, this, [this, edit, placeholder] { editScript(*edit, placeholder); });
        connect(folderButton, &QPushButton::clicked, this, [this, edit, placeholder] { openScriptFolder(*edit, placeholder); });
    };
    addScriptRow(tr("Run at NereusSDR start"), &m_startEnabled, &m_startPath,
                 ScriptRunner::defaultStartScript());
    addScriptRow(tr("Run at NereusSDR exit"), &m_stopEnabled, &m_stopPath,
                 ScriptRunner::defaultStopScript());

    auto* delayRow = new QHBoxLayout;
    delayRow->addWidget(new QLabel(tr("Delay auto-connect by"), this));
    m_delay = new QSpinBox(this);
    m_delay->setRange(0, 300);
    m_delay->setSuffix(tr(" s"));
    delayRow->addWidget(m_delay);
    auto* delayNote = new QLabel(tr("(0 = immediately). Gives the SDR time to boot after the start script."), this);
    delayNote->setWordWrap(true);
    delayRow->addWidget(delayNote, 1);
    scriptLayout->addLayout(delayRow);

    auto* defaults = new QLabel(tr("Defaults (used when the path is blank):\n%1\n%2")
                                    .arg(ScriptRunner::defaultStartScript(),
                                         ScriptRunner::defaultStopScript()), scriptGroup);
    defaults->setWordWrap(true);
    defaults->setTextInteractionFlags(Qt::TextSelectableByMouse);
    scriptLayout->addWidget(defaults);

    auto* removeScripts = smallButton(tr("Remove installed scripts"), scriptGroup);
    scriptLayout->addWidget(removeScripts);
    connect(removeScripts, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Remove scripts"),
                                  tr("Remove the installed start/exit scripts and the Denkovi tool "
                                     "from your user folder? They are copied again if you enable a "
                                     "script later."),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        QString error;
        if (!ScriptRunner::removeInstalled(&error)) {
            QMessageBox::warning(this, tr("Remove scripts"), error);
        }
    });

    connect(m_startEnabled, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_syncing) { saveStartEnabled(on); }
    });
    connect(m_stopEnabled, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_syncing) { saveStopEnabled(on); }
    });
    connect(m_startPath, &QLineEdit::editingFinished, this, [this] {
        if (!m_syncing) { saveStartPath(); }
    });
    connect(m_stopPath, &QLineEdit::editingFinished, this, [this] {
        if (!m_syncing) { saveStopPath(); }
    });
    connect(m_delay, &QSpinBox::valueChanged, this, [](int value) {
        AppSettings::instance().setValue(QStringLiteral("Scripts/PowerOnDelay"), value);
    });
    syncScripts();
    updateHelperStatus();
}

void StationHelperOptions::updateHelperStatus()
{
    HelperApps& helpers = HelperApps::instance();
    const auto describe = [&helpers](HelperApps::Helper helper, const QString& fault) {
        if (helper == HelperApps::Helper::WaveNode) {
            const bool enabled = helpers.enabled(helper);
            const bool receiving = helpers.isRunning(helper);
            if (!fault.isEmpty()) {
                return tr("Fault: %1").arg(fault);
            }
            if (!enabled) {
                return tr("Independent helper - listening disabled (Containers \u2192 Helpers)");
            }
            return receiving
                ? tr("Independent helper - receiving data; start WaveNodeUsb.exe yourself")
                : tr("Independent helper - waiting for WaveNodeUsb.exe; start it yourself");
        }
        const bool enabled = helpers.enabled(helper);
        const bool running = helpers.isRunning(helper);
        const QString path = HelperApps::executablePath(helper);
        const bool installed = QFileInfo::exists(path);
        const QString state = !fault.isEmpty() ? tr("Fault: %1").arg(fault)
            : !enabled ? tr("Disabled (Containers → Helpers)")
            : running ? tr("Enabled, running")
            : installed ? tr("Enabled, stopped")
            : tr("Enabled, installs when started");
        return state + QStringLiteral("  \u00b7  ") + path;
    };
    if (m_waveStatus) {
        m_waveStatus->setText(describe(HelperApps::Helper::WaveNode, m_waveFault));
        m_waveStatus->setStyleSheet(m_waveFault.isEmpty() ? QString()
            : QStringLiteral("color: #ff8080;"));
    }
    if (m_hfStatus) {
        m_hfStatus->setText(describe(HelperApps::Helper::HfAuto, m_hfFault));
        m_hfStatus->setStyleSheet(m_hfFault.isEmpty() ? QString()
            : QStringLiteral("color: #ff8080;"));
    }
}

void StationHelperOptions::syncScripts()
{
    AppSettings& settings = AppSettings::instance();
    m_syncing = true;
    m_startEnabled->setChecked(settings.value(QStringLiteral("Scripts/StartEnabled"), false).toBool());
    m_stopEnabled->setChecked(settings.value(QStringLiteral("Scripts/StopEnabled"), false).toBool());
    m_startPath->setText(settings.value(QStringLiteral("Scripts/StartPath")).toString());
    m_stopPath->setText(settings.value(QStringLiteral("Scripts/StopPath")).toString());
    m_delay->setValue(settings.value(QStringLiteral("Scripts/PowerOnDelay"), 2).toInt());
    m_syncing = false;
}

void StationHelperOptions::saveStartEnabled(bool on)
{
    AppSettings::instance().setValue(QStringLiteral("Scripts/StartEnabled"), on);
    if (on) {
        QString error;
        if (!ScriptRunner::seed(&error)) { QMessageBox::warning(this, tr("Start script"), error); }
    }
}

void StationHelperOptions::saveStopEnabled(bool on)
{
    AppSettings::instance().setValue(QStringLiteral("Scripts/StopEnabled"), on);
    if (on) {
        QString error;
        if (!ScriptRunner::seed(&error)) { QMessageBox::warning(this, tr("Exit script"), error); }
    }
}

void StationHelperOptions::saveStartPath()
{
    AppSettings::instance().setValue(QStringLiteral("Scripts/StartPath"), m_startPath->text().trimmed());
}

void StationHelperOptions::saveStopPath()
{
    AppSettings::instance().setValue(QStringLiteral("Scripts/StopPath"), m_stopPath->text().trimmed());
}

void StationHelperOptions::browse(QLineEdit* edit)
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select a start/exit script"), edit->text(),
        tr("Scripts and programs (*.bat *.cmd *.exe);;All files (*)"));
    if (path.isEmpty()) { return; }
    edit->setText(QDir::toNativeSeparators(path));
    if (edit == m_startPath) { saveStartPath(); } else { saveStopPath(); }
}

void StationHelperOptions::editScript(QLineEdit* edit, const QString& fallback)
{
    QString file = edit->text().trimmed();
    if (file.isEmpty()) { file = fallback; }
    if (!QFileInfo::exists(file)) {
        QDir().mkpath(QFileInfo(file).absolutePath());
        QFile starter(file);
        if (starter.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            starter.write("@echo off\r\nrem Add your commands here.\r\nexit\r\n");
        }
    }
    QProcess::startDetached(QStringLiteral("notepad.exe"), { QDir::toNativeSeparators(file) });
}

void StationHelperOptions::openScriptFolder(QLineEdit* edit, const QString& fallback)
{
    QString file = edit->text().trimmed();
    if (file.isEmpty()) { file = fallback; }
    if (QFileInfo::exists(file)) {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QStringLiteral("/select,") + QDir::toNativeSeparators(file) });
    } else {
        const QString dir = QFileInfo(file).absolutePath();
        QDir().mkpath(dir);
        QProcess::startDetached(QStringLiteral("explorer.exe"), { QDir::toNativeSeparators(dir) });
    }
}

} // namespace NereusSDR
