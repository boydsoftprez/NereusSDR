// no-port-check: NereusSDR-original native CAT widgets, model synchronization and lifecycle.
// Thetis setup.Designer.cs defines the referenced choice inventories, not this Qt UI logic.
// Native SetupPage/TCI styling and AetherSDR CatControlApplet were studied; no new UI port.
// Modification history (NereusSDR):
// 2026-10-04 - Keep disabled PTY platform reasons and remote-host guidance.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-06 - Warn when a listener is open beyond this computer; ports apply when
//              typing finishes. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-06 - Notes, tooltips and section names in operator words.
//              J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-07 - Read and change CAT through RadioModel::catControl(), so a
//              connected desktop sets up the Core's CAT; the platform choices
//              are the Core's there. Review fixes: each page syncs on the
//              changes it shows, a slice picked rebinds, a typed device path
//              survives a refresh, the tester's refusal is the page's.
//              J.J. Boyd (KG4VCF), AI tooling: Claude Code.
// 2026-10-07 - The PTY dialects from the one list in core/cat/CatPtyDialects.h.
//              J.J. Boyd (KG4VCF), AI tooling: Claude Code.
// 2026-10-08 - A Serial Ports or TCP/IP CAT page alive when its RadioModel is
//              destroyed (quit with Setup open) stops listening to CAT, so
//              CatService's teardown no longer reads deleted slices.
//              J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "CatNetworkSetupPages.h"
#include "core/cat/CatControl.h"
#include "core/cat/CatPtyDialects.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/SliceOwnership.h"
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHostAddress>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
namespace NereusSDR {
namespace {
// rigctld(1) warns the protocol has no authentication; the Thetis TCP CAT has none either.
QString openToNetwork(bool enabled, const QString& address) {
    const QHostAddress bind(address);
    if (!enabled || bind.isNull() || bind.isLoopback()) { return {}; }
    return QStringLiteral("\n") + QObject::tr("Open to your network: any device that can reach this port can tune and key the radio. There is no password.");
}
QStringList baudChoices() {
    // From Thetis setup.Designer.cs:57973-57983 [v2.10.3.15]. Choice facts only.
    return {"300","1200","2400","4800","9600","19200","38400","57600","115200"};
}
QComboBox* combo(QWidget* parent, const QStringList& items, const QString& name) {
    auto* widget = new QComboBox(parent); widget->addItems(items); widget->setObjectName(name);
    widget->setStyleSheet(QString::fromLatin1(Style::kComboStyle)); return widget;
}
void setChoice(QComboBox* widget, const QString& text) {
    const QSignalBlocker blocked(widget);
    if (widget->findText(text) < 0) { widget->addItem(text); }
    widget->setCurrentText(text);
}
void choiceAvailable(QComboBox* widget, const QString& value, bool available, const QString& reason) {
    const int index = widget->findText(value);
    if (index < 0) { return; }
    if (auto* items = qobject_cast<QStandardItemModel*>(widget->model())) {
        items->item(index)->setEnabled(available); items->item(index)->setToolTip(available ? QString() : reason);
    }
}
void unavailableChoice(QComboBox* widget, const QString& value, const QString& reason) { choiceAvailable(widget,value,false,reason); }
void serialPlatformChoices(QComboBox* parity, QComboBox* stops, const CatPlatform& platform, bool remote) {
    // Native Qt capability facts: https://doc.qt.io/qt-6/qserialport.html#parity-prop
    // and https://doc.qt.io/qt-6/qserialport.html#StopBits-enum
    // The computer running CAT says which it offers (the Core's, from a connected desktop).
    choiceAvailable(parity,"Mark",platform.markSpaceParity,remote ? QObject::tr("The Core's computer cannot use mark parity.") : QObject::tr("Mark parity is unavailable in QtSerialPort on macOS."));
    choiceAvailable(parity,"Space",platform.markSpaceParity,remote ? QObject::tr("The Core's computer cannot use space parity.") : QObject::tr("Space parity is unavailable in QtSerialPort on macOS."));
    choiceAvailable(stops,"1.5",platform.oneAndHalfStop,remote ? QObject::tr("The Core's computer cannot use 1.5 stop bits.") : QObject::tr("1.5 stop bits are available only on Windows."));
}
QString noSerialReason(bool remote) {
    return remote ? QObject::tr("The Core was built without serial port support.") : QObject::tr("This copy of NereusSDR was built without serial port support.");
}
QString noPtyReason(bool remote) {
    return remote ? QObject::tr("The Core's computer has no native PTYs: they are available only on macOS and Linux.") : QObject::tr("Native PTYs are available only on macOS and Linux; use a supplied virtual COM device here.");
}
QFormLayout* sectionForm(QGroupBox* group) {
    auto* form=new QFormLayout; form->setVerticalSpacing(6);
    qobject_cast<QVBoxLayout*>(group->layout())->addLayout(form);
    return form;
}
QLabel* note(QWidget* parent, const QString& text) {
    auto* label = new QLabel(text,parent); label->setWordWrap(true);
    label->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle)); return label;
}
// The page's note while CAT cannot be set up from this window, with the reason.
void showUnavailable(QLabel* label, CatControl* control) {
    const bool available = control && control->available();
    label->setText(available || !control ? QString() : control->unavailableReason());
    label->setVisible(!label->text().isEmpty());
}
// Marks the "Invalid binding" entry: picking it binds nothing anew.
constexpr int kInvalidRole=Qt::UserRole+2;
// The selector's entry names a slice to bind now (not None, not the closed one).
bool pickedSlice(const QComboBox* widget) {
    return widget->currentData().toInt()>=0 && !widget->currentData(kInvalidRole).toBool();
}
// `valid`: the binding still names the slice it was bound to (the Core's
// own test, from a connected desktop).
void fillSlices(QComboBox* widget, RadioModel* model, int selected, quint64 incarnation, bool valid) {
    const QSignalBlocker blocked(widget); widget->clear(); widget->addItem(QObject::tr("None"),-1);
    widget->setItemData(0,QVariant::fromValue(quint64(0)),Qt::UserRole+1);
    int index=selected<0 ? 0:-1;
    if (model) {
        for (SliceModel* slice:model->slices()) {
            const int id=slice->sliceIndex(); const quint64 live=model->sliceOwnership()->incarnation(id);
            widget->addItem(QObject::tr("Slice ID %1").arg(id),id);
            widget->setItemData(widget->count()-1,QVariant::fromValue(live),Qt::UserRole+1);
            if (id==selected && valid) { index=widget->count()-1; }
        }
    }
    if (index<0 && valid) {
        // A live slice this window does not show (a connected desktop's slices arrive later).
        widget->addItem(QObject::tr("Slice ID %1").arg(selected),selected); index=widget->count()-1;
        widget->setItemData(index,QVariant::fromValue(incarnation),Qt::UserRole+1);
    }
    if (index<0) {
        widget->addItem(QObject::tr("Invalid binding — ID %1").arg(selected),selected); index=widget->count()-1;
        widget->setItemData(index,QVariant::fromValue(incarnation),Qt::UserRole+1); widget->setItemData(index,true,kInvalidRole);
        unavailableChoice(widget,widget->itemText(index),QObject::tr("The slice this channel controlled was closed. Pick another slice."));
    }
    widget->setCurrentIndex(index);
    widget->setToolTip(index>0 && widget->itemText(index).startsWith(QObject::tr("Invalid binding"))
        ? QObject::tr("The slice this channel controlled was closed. Pick another slice.") : QObject::tr("This channel keeps controlling this slice, whichever slice is selected on screen."));
}
}
CatSerialPortsPage::CatSerialPortsPage(RadioModel* model, QWidget* parent) : CatChannelSetupPage(model,true,parent) {}
CatTcpIpPage::CatTcpIpPage(RadioModel* model, QWidget* parent) : CatChannelSetupPage(model,false,parent) {}
CatChannelSetupPage::CatChannelSetupPage(RadioModel* model, bool serial, QWidget* parent)
    : SetupPage(serial ? tr("Serial Ports") : tr("TCP/IP CAT"),model,parent), m_control(model ? model->catControl() : nullptr), m_serial(serial)
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));
    m_unavailable=note(this,{}); m_unavailable->setObjectName("catUnavailable"); contentLayout()->insertWidget(contentLayout()->count()-1,m_unavailable);
    for (int i=0;i<4;++i) {
        const QString prefix=QStringLiteral("cat%1").arg(i+1);
        auto* group=new QGroupBox(tr("CAT %1").arg(i+1),this); m_groups[i]=group;
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
        auto* grid=new QGridLayout(group); grid->setSpacing(6); Row& row=m_rows[i];
        row.enabled=new QCheckBox(serial ? tr("Enable serial CAT") : tr("Enable TCP CAT"),group); row.enabled->setObjectName(prefix+"Enabled");
        row.enabled->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle)); grid->addWidget(row.enabled,0,0);
        row.primary=combo(group,{},prefix+"Primary"); row.secondary=combo(group,{},prefix+"Secondary");
        grid->addWidget(new QLabel(tr("VFO A:"),group),0,1); grid->addWidget(row.primary,0,2);
        grid->addWidget(new QLabel(tr("VFO B:"),group),0,3); grid->addWidget(row.secondary,0,4);
        if (serial) {
            // The devices are those of the computer running CAT (the Core's, from a connected desktop).
            row.device=combo(group,{},prefix+"Device"); row.device->setEditable(true);
            grid->addWidget(new QLabel(tr("Device:"),group),1,0); grid->addWidget(row.device,1,1,1,4);
            row.baud=combo(group,baudChoices(),prefix+"Baud");
            // From Thetis setup.Designer.cs:58053-58058,58069-58072,58083-58086 [v2.10.3.15]. Choice facts.
            row.parity=combo(group,{"None","Odd","Even","Mark","Space"},prefix+"Parity");
            row.bits=combo(group,{"8","7","6"},prefix+"Bits"); row.stops=combo(group,{"1","1.5","2"},prefix+"Stops");
            auto* format=new QHBoxLayout; format->addWidget(new QLabel(tr("Baud:"),group)); format->addWidget(row.baud);
            format->addWidget(new QLabel(tr("Parity:"),group)); format->addWidget(row.parity);
            format->addWidget(new QLabel(tr("Bits:"),group)); format->addWidget(row.bits);
            format->addWidget(new QLabel(tr("Stops:"),group)); format->addWidget(row.stops); grid->addLayout(format,2,0,1,5);
        } else {
            row.address=new QLineEdit(group); row.address->setObjectName(prefix+"Address"); row.address->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
            row.port=new QSpinBox(group); row.port->setRange(0,65535); row.port->setSpecialValueText(tr("Choose port")); row.port->setObjectName(prefix+"Port"); row.port->setKeyboardTracking(false); row.port->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
            grid->addWidget(new QLabel(tr("Listen on:"),group),1,0); grid->addWidget(row.address,1,1,1,2);
            grid->addWidget(new QLabel(tr("Port:"),group),1,3); grid->addWidget(row.port,1,4);
            row.pty=new QCheckBox(tr("Enable PTY"),group); row.pty->setObjectName(prefix+"Pty"); row.pty->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle)); grid->addWidget(row.pty,2,0,1,2);
            row.dialect=combo(group,catPtyDialectValues(),prefix+"PtyDialect");
            grid->addWidget(row.dialect,3,0,1,2);
            row.rigctld=new QCheckBox(tr("Enable Hamlib rigctld"),group); row.rigctld->setObjectName(prefix+"RigctldEnabled");
            row.rigctld->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle)); grid->addWidget(row.rigctld,5,0,1,5);
            row.rigAddress=new QLineEdit(group); row.rigAddress->setObjectName(prefix+"RigctldAddress"); row.rigAddress->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
            row.rigPort=new QSpinBox(group); row.rigPort->setRange(0,65535); row.rigPort->setSpecialValueText(tr("Choose port")); row.rigPort->setObjectName(prefix+"RigctldPort"); row.rigPort->setKeyboardTracking(false); row.rigPort->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
            grid->addWidget(new QLabel(tr("Listen on:"),group),6,0); grid->addWidget(row.rigAddress,6,1,1,2);
            grid->addWidget(new QLabel(tr("Port:"),group),6,3); grid->addWidget(row.rigPort,6,4);
            row.rigStatus=note(group,{}); row.rigStatus->setObjectName(prefix+"RigctldStatus"); grid->addWidget(row.rigStatus,7,0,1,5);
            row.path=note(group,{}); row.path->setObjectName(prefix+"Path"); row.path->setTextInteractionFlags(Qt::TextSelectableByMouse); grid->addWidget(row.path,3,2,1,3);
        }
        row.status=note(group,{}); row.status->setObjectName(prefix+"Status"); grid->addWidget(row.status,serial ? 3 : 4,0,1,5);
        contentLayout()->insertWidget(contentLayout()->count()-1,group);
        connect(row.enabled,&QCheckBox::toggled,this,[this,i] { apply(i+1); });
        if (row.pty) { connect(row.pty,&QCheckBox::toggled,this,[this,i] { apply(i+1); }); }
        for (QComboBox* widget:{row.baud,row.parity,row.bits,row.stops,row.dialect}) {
            if (!widget) { continue; } widget->installEventFilter(this);
            connect(widget,&QComboBox::currentIndexChanged,this,[this,i] { apply(i+1); });
        }
        // A slice picked in a selector binds to that slice now, even one closed and opened again with its id.
        row.primary->installEventFilter(this); row.secondary->installEventFilter(this);
        connect(row.primary,&QComboBox::currentIndexChanged,this,[this,i] { apply(i+1,CatRebind{pickedSlice(m_rows[i].primary),false}); });
        connect(row.secondary,&QComboBox::currentIndexChanged,this,[this,i] { apply(i+1,CatRebind{false,pickedSlice(m_rows[i].secondary)}); });
        if (row.device) { row.device->installEventFilter(this); connect(row.device->lineEdit(),&QLineEdit::editingFinished,this,[this,i] { apply(i+1); }); connect(row.device,&QComboBox::activated,this,[this,i] { apply(i+1); }); }
        if (row.rigctld) { connect(row.rigctld,&QCheckBox::toggled,this,[this,i] { apply(i+1); }); }
        if (row.rigAddress) { connect(row.rigAddress,&QLineEdit::editingFinished,this,[this,i] { apply(i+1); }); }
        if (row.rigPort) { row.rigPort->installEventFilter(this); connect(row.rigPort,&QSpinBox::valueChanged,this,[this,i] { apply(i+1); }); }
        if (row.address) { connect(row.address,&QLineEdit::editingFinished,this,[this,i] { apply(i+1); }); }
        if (row.port) { row.port->installEventFilter(this); connect(row.port,&QSpinBox::valueChanged,this,[this,i] { apply(i+1); }); }
    }
    if (!serial) {
        contentLayout()->insertWidget(contentLayout()->count()-1,note(this,tr("Each channel controls the slices assigned to it, whichever slice is selected on screen. A virtual serial port exists only while its channel is on.")));
    }
    if (m_control) {
        // The changes this page shows: a channel's settings and live state, and whether CAT can be set up here.
        connect(m_control,&CatControl::channelConfigChanged,this,[this] { syncFromModel(); });
        connect(m_control,&CatControl::channelStatusChanged,this,[this] { syncFromModel(); });
        connect(m_control,&CatControl::availabilityChanged,this,[this] { syncFromModel(); });
        connect(m_control,&CatControl::platformChanged,this,[this] { syncPlatform(true); });
    }
    if (model) { connect(model,&RadioModel::sliceAdded,this,[this] { syncFromModel(); }); connect(model,&RadioModel::sliceRemoved,this,[this] { syncFromModel(); }); }
    if (model) {
        // On quit MainWindow destroys its RadioModel before the Setup dialog.
        // The model's slices are gone when destroyed() fires, and CatService
        // (a child, deleted after it) then reports each channel stopped: the
        // page stops listening here instead of reading slices that are gone.
        connect(model,&QObject::destroyed,this,[this] {
            if (m_control) { disconnect(m_control,nullptr,this,nullptr); }
            m_control.clear();
        });
    }
    syncFromModel();
}
bool CatChannelSetupPage::eventFilter(QObject* watched,QEvent* event) {
    if (event->type()==QEvent::Wheel) { return true; } return SetupPage::eventFilter(watched,event);
}
void CatChannelSetupPage::showEvent(QShowEvent* event) {
    // The device box lists the serial devices there are now.
    if (m_serial && m_control) { m_control->refreshDevices(); }
    SetupPage::showEvent(event);
}
void CatChannelSetupPage::apply(int channel, CatRebind rebind) {
    if (m_syncing || !m_control || !m_control->available()) { return; }
    const Row& row=m_rows[channel-1]; CatEndpointConfig config=m_control->channelConfig(channel);
    config.binding.primarySliceId=row.primary->currentData().toInt(); const int secondary=row.secondary->currentData().toInt();
    config.binding.secondarySliceId=secondary<0 ? std::nullopt : std::optional<int>(secondary);
    config.binding.primaryIncarnation=row.primary->currentData(Qt::UserRole+1).toULongLong();
    config.binding.secondaryIncarnation=secondary<0 ? std::nullopt : std::optional<quint64>(row.secondary->currentData(Qt::UserRole+1).toULongLong());
    if (m_serial) { config.serialEnabled=row.enabled->isChecked(); config.serialDevice=row.device->currentText().trimmed(); config.serialBaud=row.baud->currentText().toInt(); config.serialParity=row.parity->currentText(); config.serialDataBits=row.bits->currentText().toInt(); config.serialStopBits=row.stops->currentText(); }
    else { config.tcpEnabled=row.enabled->isChecked(); config.tcpBindAddress=row.address->text().trimmed(); config.tcpPort=row.port->value(); config.ptyEnabled=row.pty->isChecked(); config.ptyDialect=row.dialect->currentText();
        config.rigctldEnabled=row.rigctld->isChecked(); config.rigctldBindAddress=row.rigAddress->text().trimmed(); config.rigctldPort=row.rigPort->value(); }
    const QPointer<CatChannelSetupPage> lifetime(this); const bool remote=m_control->remote();
    m_control->reconfigureChannel(channel,config,[lifetime,channel,remote](bool accepted,const QString& reason) {
        if (!lifetime) { return; }
        // The Core's new settings follow its answer; until then the page keeps what was chosen.
        if (!accepted || !remote) { lifetime->syncFromModel(); }
        if (lifetime && !accepted) { lifetime->m_rows[channel-1].status->setText(reason); }
    },this,rebind);
}
void CatChannelSetupPage::syncFromModel() {
    if (!m_control) { return; } m_syncing=true;
    const bool available=m_control->available(); const QString reason=m_control->unavailableReason();
    showUnavailable(m_unavailable,m_control);
    for (int i=0;i<4;++i) {
        Row& row=m_rows[i]; const CatEndpointConfig config=m_control->channelConfig(i+1); const CatChannelStatus status=m_control->channelStatus(i+1);
        row.enabled->setToolTip(!available ? reason : QString());
        fillSlices(row.primary,model(),config.binding.primarySliceId,config.binding.primaryIncarnation,status.primaryValid); fillSlices(row.secondary,model(),config.binding.secondarySliceId.value_or(-1),config.binding.secondaryIncarnation.value_or(0),status.secondaryValid);
        { const QSignalBlocker block(row.enabled); row.enabled->setChecked(m_serial ? config.serialEnabled : config.tcpEnabled); }
        if (m_serial) {
            setChoice(row.device,config.serialDevice); setChoice(row.baud,QString::number(config.serialBaud)); setChoice(row.parity,config.serialParity); setChoice(row.bits,QString::number(config.serialDataBits)); setChoice(row.stops,config.serialStopBits); row.status->setText(tr("Serial: %1").arg(status.serial));
        }
        else {
            const QSignalBlocker address(row.address), port(row.port), pty(row.pty);
            const QSignalBlocker rigEnabled(row.rigctld), rigAddress(row.rigAddress), rigPort(row.rigPort);
            row.rigctld->setChecked(config.rigctldEnabled); row.rigAddress->setText(config.rigctldBindAddress); row.rigPort->setValue(config.rigctldPort); setChoice(row.dialect,config.ptyDialect);
            row.rigStatus->setText(tr("Rigctld: %1 · Bound: %2:%3 · Clients: %4").arg(status.rigctld,status.rigctldBoundAddress).arg(status.rigctldBoundPort).arg(status.rigctldClients)+openToNetwork(config.rigctldEnabled,config.rigctldBindAddress));
            row.address->setText(config.tcpBindAddress); row.port->setValue(config.tcpPort); row.pty->setChecked(config.ptyEnabled);
            row.path->setText(status.ptyPath.isEmpty() ? tr("PTY: %1").arg(status.pty) : status.ptyPath);
            row.status->setText(tr("TCP: %1 · Bound: %2:%3 · Clients: %4").arg(status.tcp,status.tcpBoundAddress).arg(status.tcpBoundPort).arg(status.tcpClients)+openToNetwork(config.tcpEnabled,config.tcpBindAddress));
        }
    }
    m_syncing=false;
    syncPlatform(false);
}
void CatChannelSetupPage::syncPlatform(bool keepTyped) {
    if (!m_control) { return; }
    const bool wasSyncing=m_syncing; m_syncing=true;
    const bool available=m_control->available(); const bool remote=m_control->remote(); const QString reason=m_control->unavailableReason();
    const CatPlatform platform=m_control->platform();
    for (int i=0;i<4;++i) {
        Row& row=m_rows[i]; const CatEndpointConfig config=m_control->channelConfig(i+1);
        const bool serialMissing=m_serial && available && !platform.serial;
        m_groups[i]->setEnabled(available && !serialMissing);
        m_groups[i]->setToolTip(!available ? reason : serialMissing ? noSerialReason(remote) : QString());
        if (m_serial) {
            // The devices are those of the computer running CAT; the path in the box, typed or picked, stays.
            if (row.device->property("devices").toStringList()!=platform.serialDevices) {
                const QString shown=keepTyped ? row.device->currentText() : config.serialDevice;
                const QSignalBlocker blocked(row.device); row.device->clear(); row.device->addItems(platform.serialDevices); row.device->setProperty("devices",platform.serialDevices);
                if (keepTyped) { row.device->setCurrentText(shown); } else { setChoice(row.device,shown); }
            }
            serialPlatformChoices(row.parity,row.stops,platform,remote);
        } else {
            // PTYs are the computer running CAT's (the Core's, from a connected desktop).
            if (available && !platform.pty) { row.pty->setEnabled(false); row.pty->setToolTip(noPtyReason(remote)); }
            else {
                row.pty->setEnabled(true);
                row.pty->setToolTip(!available ? reason : remote ? tr("CAT %1 PTY uses %2 commands on the Core's computer.").arg(i+1).arg(config.ptyDialect) : tr("CAT %1 PTY uses %2 commands on this computer.").arg(i+1).arg(config.ptyDialect));
            }
        }
    }
    m_syncing=wasSyncing;
}
CatGlobalSetupPage::CatGlobalSetupPage(const QString& title,RadioModel* model,QWidget* parent)
    : SetupPage(title,model,parent),m_control(model ? model->catControl() : nullptr) {
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));
    m_unavailable=note(this,{}); m_unavailable->setObjectName("catUnavailable"); contentLayout()->insertWidget(contentLayout()->count()-1,m_unavailable);
    m_refused=note(this,{}); m_refused->setObjectName("catRefused"); m_refused->setVisible(false); contentLayout()->insertWidget(contentLayout()->count()-1,m_refused);
    if (m_control) {
        // The global settings and whether CAT can be set up here; the PTT state and the platform only touch their own controls.
        connect(m_control,&CatControl::globalConfigChanged,this,[this] { syncFromModel(); });
        connect(m_control,&CatControl::availabilityChanged,this,[this] { syncFromModel(); });
        connect(m_control,&CatControl::pttStateChanged,this,[this] { syncPtt(); });
        connect(m_control,&CatControl::platformChanged,this,[this] { syncPlatform(); });
    }
}
bool CatGlobalSetupPage::eventFilter(QObject* watched,QEvent* event) { if (event->type()==QEvent::Wheel) { return true; } return SetupPage::eventFilter(watched,event); }
QCheckBox* CatGlobalSetupPage::addCheck(QFormLayout* form,const QString& label,const QString& name,bool CatGlobalConfig::* field) {
    auto* widget=new QCheckBox(label,this); widget->setMinimumHeight(Style::kButtonH); widget->setObjectName(name); widget->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle)); m_controls.push_back(widget); form->addRow(widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { const QSignalBlocker blocked(widget); widget->setChecked(config.*field); });
    connect(widget,&QCheckBox::toggled,this,[this,field](bool value) { if (m_syncing || !m_control) { return; } CatGlobalConfig config=m_control->globalConfig(); config.*field=value; applyConfiguration(config); }); return widget;
}
QComboBox* CatGlobalSetupPage::addChoice(QFormLayout* form,const QString& label,const QString& name,const QStringList& items,QString CatGlobalConfig::* field) {
    auto* widget=combo(this,items,name); m_controls.push_back(widget); widget->installEventFilter(this); form->addRow(label,widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { setChoice(widget,config.*field); });
    connect(widget,&QComboBox::currentTextChanged,this,[this,field](const QString& value) { if (m_syncing || !m_control) { return; } CatGlobalConfig config=m_control->globalConfig(); config.*field=value; applyConfiguration(config); }); return widget;
}
QLineEdit* CatGlobalSetupPage::addText(QFormLayout* form,const QString& label,const QString& name,QString CatGlobalConfig::* field) {
    auto* widget=new QLineEdit(this); widget->setObjectName(name); widget->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle)); m_controls.push_back(widget); form->addRow(label,widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { const QSignalBlocker blocked(widget); widget->setText(config.*field); });
    connect(widget,&QLineEdit::editingFinished,this,[this,widget,field] { if (m_syncing || !m_control) { return; } CatGlobalConfig config=m_control->globalConfig(); config.*field=widget->text(); applyConfiguration(config); }); return widget;
}
QSpinBox* CatGlobalSetupPage::addNumber(QFormLayout* form,const QString& label,const QString& name,int low,int high,int CatGlobalConfig::* field) {
    auto* widget=new QSpinBox(this); widget->setObjectName(name); widget->setRange(low,high); widget->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle)); m_controls.push_back(widget); widget->installEventFilter(this); form->addRow(label,widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { const QSignalBlocker blocked(widget); widget->setValue(config.*field); });
    connect(widget,&QSpinBox::valueChanged,this,[this,field](int value) { if (m_syncing || !m_control) { return; } CatGlobalConfig config=m_control->globalConfig(); config.*field=value; applyConfiguration(config); }); return widget;
}
void CatGlobalSetupPage::applyConfiguration(const CatGlobalConfig& config) {
    if (!m_control || !m_control->available()) { return; }
    const QPointer<CatGlobalSetupPage> lifetime(this); const bool remote=m_control->remote();
    m_control->reconfigureGlobal(config,[lifetime,remote](bool accepted,const QString& reason) {
        if (!lifetime) { return; }
        // The Core's new settings follow its answer; until then the page keeps what was chosen.
        if (!accepted || !remote) { lifetime->syncFromModel(); }
        if (!lifetime) { return; }
        if (!accepted && lifetime->m_status) { lifetime->m_status->setText(reason); }
        else if (lifetime->m_refused && remote) { lifetime->m_refused->setText(accepted ? QString() : reason); lifetime->m_refused->setVisible(!accepted); }
    },this);
}
void CatGlobalSetupPage::syncFromModel() {
    if (!m_control) { return; } m_syncing=true;
    const bool available=m_control->available(); const QString reason=m_control->unavailableReason();
    showUnavailable(m_unavailable,m_control);
    for (QWidget* widget:m_controls) { widget->setEnabled(available); widget->setToolTip(available ? QString() : reason); }
    const CatGlobalConfig config=m_control->globalConfig(); for (const auto& update:m_updates) { update(config); }
    m_syncing=false;
    syncPlatform(); syncPtt();
}
void CatGlobalSetupPage::syncPlatform() {
    if (!m_control) { return; }
    const bool wasSyncing=m_syncing; m_syncing=true;
    const bool available=m_control->available(); const bool remote=m_control->remote(); const QString reason=m_control->unavailableReason();
    const CatPlatform platform=m_control->platform();
    if (m_serialGroup) {
        const bool serialMissing=available && !platform.serial;
        m_serialGroup->setEnabled(available && !serialMissing); m_serialGroup->setToolTip(!available ? reason : serialMissing ? noSerialReason(remote) : QString());
        if (m_noSerialNote) {
            m_noSerialNote->setText(remote ? tr("Input PTT is not available: the Core was built without serial port support.") : tr("Input PTT is not available: this copy of NereusSDR was built without serial port support."));
            m_noSerialNote->setVisible(serialMissing);
        }
    }
    if (m_parity && m_stops) { serialPlatformChoices(m_parity,m_stops,platform,remote); }
    m_syncing=wasSyncing;
}
void CatGlobalSetupPage::syncPtt() {
    if (m_control && m_status) { m_status->setText(tr("PTT: %1").arg(m_control->pttState())); }
}
CatOptionsSetupPage::CatOptionsSetupPage(RadioModel* model,QWidget* parent) : CatGlobalSetupPage(tr("CAT Options"),model,parent) {
    auto* options=addSection(tr("Compatibility and automatic information")); auto* form=sectionForm(options);
    // From Thetis setup.Designer.cs:59524-59528 [v2.10.3.15]. Rig identity choice facts.
    addChoice(form,tr("Report identity:"),"catRigIdentity",{"PowerSDR","TS-2000","TS-50S","TS-480"},&CatGlobalConfig::rigIdentity);
    addText(form,tr("ZZSN serial number:"),"catSerialNumber",&CatGlobalConfig::serialNumber);
    addCheck(form,tr("Send TCP welcome banner"),"catWelcome",&CatGlobalConfig::sendWelcome);
    addCheck(form,tr("Allow Kenwood AI command"),"catAllowAi",&CatGlobalConfig::allowKenwoodAi);
    addCheck(form,tr("Enable automatic frequency information"),"catAi",&CatGlobalConfig::aiEnabled);
    addCheck(form,tr("AI to TCP clients"),"catAiTcp",&CatGlobalConfig::aiTcp);
    addCheck(form,tr("AI to serial CAT 1"),"catAiSerial1",&CatGlobalConfig::aiSerial1);
    addCheck(form,tr("AI to serial CAT 2"),"catAiSerial2",&CatGlobalConfig::aiSerial2);
    addCheck(form,tr("AI to serial CAT 3"),"catAiSerial3",&CatGlobalConfig::aiSerial3);
    addCheck(form,tr("AI to serial CAT 4"),"catAiSerial4",&CatGlobalConfig::aiSerial4);
    addCheck(form,tr("Report DIGL / DIGU as LSB / USB"),"catDigitalSideband",&CatGlobalConfig::digitalReportsSideband);
    addCheck(form,tr("Apply power limits to CAT power queries"),"catLimitPower",&CatGlobalConfig::limitReportedPower);
    auto* recenter=new QCheckBox(tr("Always recenter VFOs"),options); recenter->setEnabled(false); recenter->setToolTip(tr("A frequency from CAT moves the panadapter the way tuning by hand does: it follows the VFO, and with CTUN on it stays put unless the band changes.")); form->addRow(recenter);
    auto* rtty=addSection(tr("RTTY frequency reporting")); auto* offsets=sectionForm(rtty);
    addCheck(offsets,tr("Apply offset to VFO A"),"catRttyA",&CatGlobalConfig::rttyOffsetAEnabled);
    addCheck(offsets,tr("Apply offset to VFO B"),"catRttyB",&CatGlobalConfig::rttyOffsetBEnabled);
    addNumber(offsets,tr("DIGU offset (Hz):"),"catRttyDigu",CatDefaults::kRttyMinimumHz,CatDefaults::kRttyMaximumHz,&CatGlobalConfig::rttyDiguHz);
    addNumber(offsets,tr("DIGL offset (Hz):"),"catRttyDigl",CatDefaults::kRttyMinimumHz,CatDefaults::kRttyMaximumHz,&CatGlobalConfig::rttyDiglHz);
    auto* testing=addSection(tr("CAT Tester")); auto* testForm=sectionForm(testing);
    auto* channel=combo(testing,{"1","2","3","4"},"catTesterChannel"); channel->installEventFilter(this); testForm->addRow(tr("Channel:"),channel);
    auto* command=new QLineEdit("ID;",testing); command->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle)); command->setObjectName("catTesterCommand"); testForm->addRow(tr("Command:"),command);
    auto* reply=note(testing,{}); reply->setMinimumWidth(200); reply->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred); reply->setObjectName("catTesterReply"); reply->setTextInteractionFlags(Qt::TextSelectableByMouse); testForm->addRow(tr("Reply:"),reply);
    auto* send=new QPushButton(tr("Test command"),testing); send->setAutoDefault(false); send->setStyleSheet(QString::fromLatin1(Style::kButtonStyle)); send->setObjectName("catTesterSend"); m_controls.push_back(send); testForm->addRow(send);
    connect(send,&QPushButton::clicked,this,[this,channel,command,reply] {
        if (!m_control) { return; }
        const QPointer<CatOptionsSetupPage> lifetime(this); const QPointer<QLabel> output(reply);
        // A refusal is said here, so the window does not also say it.
        m_control->testCommand(channel->currentText().toInt(),command->text().toLatin1(),[lifetime,output](bool ran,const QByteArray& bytes,const QString& reason) {
            if (lifetime && output) { output->setText(!ran ? reason : bytes.isEmpty() ? tr("Accepted (no reply)") : QString::fromLatin1(bytes)); }
        },this);
    });
    testForm->addRow(note(testing,tr("Test commands act on the radio: receive and setting changes take effect. Commands that transmit (TX, Tune, Two Tone, VOX on, PureSignal single shot and calibration) are refused here.")));
    auto* log=new QPushButton(tr("Show CAT Log…"),testing); log->setObjectName("catShowLog"); log->setAutoDefault(false); log->setStyleSheet(QString::fromLatin1(Style::kButtonStyle)); m_controls.push_back(log); connect(log,&QPushButton::clicked,this,&CatOptionsSetupPage::showLogRequested); testForm->addRow(log);
    syncFromModel();
}
CatPttSetupPage::CatPttSetupPage(RadioModel* model,QWidget* parent) : CatGlobalSetupPage(tr("CAT PTT"),model,parent) {
    auto* group=addSection(tr("Input PTT")); auto* form=sectionForm(group); m_serialGroup=group;
    addCheck(form,tr("Enable input PTT"),"catPttEnabled",&CatGlobalConfig::pttEnabled);
    addChoice(form,tr("Input source:"),"catPttSource",{"None","CAT1","CAT2","CAT3","CAT4","Physical"},&CatGlobalConfig::pttDeviceSource);
    addCheck(form,tr("Legacy RTS wiring — samples CTS input"),"catPttCts",&CatGlobalConfig::pttUseCts);
    addCheck(form,tr("Legacy DTR wiring — samples DSR input"),"catPttDsr",&CatGlobalConfig::pttUseDsr);
    addText(form,tr("Separate physical device:"),"catPttDevice",&CatGlobalConfig::pttSerialDevice);
    addNumber(form,tr("Physical device's CAT channel:"),"catPttChannel",1,4,&CatGlobalConfig::pttChannel);
    // Source serial choices are the same as the ordinary CAT ports, cited above.
    auto* baud=combo(group,baudChoices(),"catPttBaud"); baud->installEventFilter(this); m_controls.push_back(baud); form->addRow(tr("Baud:"),baud); m_updates.emplace_back([baud](const CatGlobalConfig& c) { setChoice(baud,QString::number(c.pttSerialBaud)); });
    connect(baud,&QComboBox::currentTextChanged,this,[this](const QString& text) { if (!m_syncing && m_control) { CatGlobalConfig c=m_control->globalConfig(); c.pttSerialBaud=text.toInt(); applyConfiguration(c); } });
    m_parity=addChoice(form,tr("Parity:"),"catPttParity",{"None","Odd","Even","Mark","Space"},&CatGlobalConfig::pttSerialParity);
    // Qt Data5–Data8: https://doc.qt.io/qt-6/qserialport.html#DataBits-enum
    addNumber(form,tr("Data bits:"),"catPttBits",5,8,&CatGlobalConfig::pttSerialDataBits);
    m_stops=addChoice(form,tr("Stop bits:"),"catPttStops",{"1","1.5","2"},&CatGlobalConfig::pttSerialStopBits);
    m_status=note(group,{}); m_status->setObjectName("catPttStatus"); form->addRow(m_status);
    form->addRow(note(group,tr("CAT1–4 read the pins of a serial CAT port that is already open. Physical opens a serial device of its own. After a press, every selected input must be released before the next press can key the radio. Opening this page or restoring settings never keys the radio. These controls never drive the RTS or DTR pins.")));
    // Shown while the computer running CAT has no serial port support.
    m_noSerialNote=note(this,{}); m_noSerialNote->setVisible(false); contentLayout()->insertWidget(contentLayout()->count()-1,m_noSerialNote);
    syncFromModel();
}
} // namespace NereusSDR
