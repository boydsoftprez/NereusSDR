// no-port-check: NereusSDR-original native CAT widgets, model synchronization and lifecycle.
// Thetis setup.Designer.cs defines the referenced choice inventories, not this Qt UI logic.
// Native SetupPage/TCI styling and AetherSDR CatControlApplet were studied; no new UI port.
// Modification history (NereusSDR):
// 2026-10-04 - J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatNetworkSetupPages.h"
#include "core/cat/CatService.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/SliceOwnership.h"
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#ifdef HAVE_SERIALPORT
#include <QSerialPortInfo>
#endif
namespace NereusSDR {
namespace {
QString localReason() { return QObject::tr("CAT listeners and setup belong to the local host. Configure CAT on the computer running the Core."); }
bool local(RadioModel* model) { return model && model->ownsLocalDsp(); }
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
void unavailableChoice(QComboBox* widget, const QString& value, const QString& reason) {
    const int index = widget->findText(value);
    if (index < 0) { return; }
    if (auto* items = qobject_cast<QStandardItemModel*>(widget->model())) {
        items->item(index)->setEnabled(false); items->item(index)->setToolTip(reason);
    }
}
void serialPlatformChoices(QComboBox* parity, QComboBox* stops) {
    // Native Qt capability facts: https://doc.qt.io/qt-6/qserialport.html#parity-prop
    // and https://doc.qt.io/qt-6/qserialport.html#StopBits-enum
#ifdef Q_OS_MAC
    unavailableChoice(parity,"Mark",QObject::tr("Mark parity is unavailable in QtSerialPort on macOS."));
    unavailableChoice(parity,"Space",QObject::tr("Space parity is unavailable in QtSerialPort on macOS."));
#endif
#ifndef Q_OS_WIN
    unavailableChoice(stops,"1.5",QObject::tr("1.5 stop bits are available only on Windows."));
#endif
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
void fillSlices(QComboBox* widget, RadioModel* model, int selected, quint64 incarnation) {
    const QSignalBlocker blocked(widget); widget->clear(); widget->addItem(QObject::tr("None"),-1);
    widget->setItemData(0,QVariant::fromValue(quint64(0)),Qt::UserRole+1);
    int index=selected<0 ? 0:-1;
    if (model) {
        for (SliceModel* slice:model->slices()) {
            const int id=slice->sliceIndex(); const quint64 live=model->sliceOwnership()->incarnation(id);
            widget->addItem(QObject::tr("Slice ID %1").arg(id),id);
            widget->setItemData(widget->count()-1,QVariant::fromValue(live),Qt::UserRole+1);
            if (id==selected && live==incarnation) { index=widget->count()-1; }
        }
    }
    if (index<0) {
        widget->addItem(QObject::tr("Invalid binding — ID %1").arg(selected),selected); index=widget->count()-1;
        widget->setItemData(index,QVariant::fromValue(incarnation),Qt::UserRole+1);
        unavailableChoice(widget,widget->itemText(index),QObject::tr("The bound slice was removed. Select a current slice explicitly to rebind."));
    }
    widget->setCurrentIndex(index);
    widget->setToolTip(index>0 && widget->itemText(index).startsWith(QObject::tr("Invalid binding"))
        ? QObject::tr("The bound slice was removed. Select a current slice explicitly to rebind.") : QObject::tr("Stable slice identity; independent of GUI focus."));
}
}
CatSerialPortsPage::CatSerialPortsPage(RadioModel* model, QWidget* parent) : CatChannelSetupPage(model,true,parent) {}
CatTcpIpPage::CatTcpIpPage(RadioModel* model, QWidget* parent) : CatChannelSetupPage(model,false,parent) {}
CatChannelSetupPage::CatChannelSetupPage(RadioModel* model, bool serial, QWidget* parent)
    : SetupPage(serial ? tr("Serial Ports") : tr("TCP/IP CAT"),model,parent), m_service(model ? model->catService() : nullptr), m_serial(serial)
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));
    if (!local(model)) { contentLayout()->insertWidget(contentLayout()->count()-1,note(this,localReason())); }
    for (int i=0;i<4;++i) {
        const QString prefix=QStringLiteral("cat%1").arg(i+1);
        auto* group=new QGroupBox(tr("CAT %1").arg(i+1),this);
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle)); group->setEnabled(local(model));
        if (!local(model)) { group->setToolTip(localReason()); }
        auto* grid=new QGridLayout(group); grid->setSpacing(6); Row& row=m_rows[i];
        row.enabled=new QCheckBox(serial ? tr("Enable serial CAT") : tr("Enable TCP CAT"),group); row.enabled->setObjectName(prefix+"Enabled");
        row.enabled->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle)); grid->addWidget(row.enabled,0,0);
        row.primary=combo(group,{},prefix+"Primary"); row.secondary=combo(group,{},prefix+"Secondary");
        grid->addWidget(new QLabel(tr("VFO A:"),group),0,1); grid->addWidget(row.primary,0,2);
        grid->addWidget(new QLabel(tr("VFO B:"),group),0,3); grid->addWidget(row.secondary,0,4);
        if (serial) {
            row.device=combo(group,{},prefix+"Device"); row.device->setEditable(true);
#ifdef HAVE_SERIALPORT
            for (const QSerialPortInfo& port:QSerialPortInfo::availablePorts()) { row.device->addItem(port.systemLocation()); }
#else
            group->setEnabled(false); group->setToolTip(tr("QtSerialPort dependency unavailable in this build."));
#endif
            grid->addWidget(new QLabel(tr("Device:"),group),1,0); grid->addWidget(row.device,1,1,1,4);
            row.baud=combo(group,baudChoices(),prefix+"Baud");
            // From Thetis setup.Designer.cs:58053-58058,58069-58072,58083-58086 [v2.10.3.15]. Choice facts.
            row.parity=combo(group,{"None","Odd","Even","Mark","Space"},prefix+"Parity");
            row.bits=combo(group,{"8","7","6"},prefix+"Bits"); row.stops=combo(group,{"1","1.5","2"},prefix+"Stops");
            serialPlatformChoices(row.parity,row.stops);
            auto* format=new QHBoxLayout; format->addWidget(new QLabel(tr("Baud:"),group)); format->addWidget(row.baud);
            format->addWidget(new QLabel(tr("Parity:"),group)); format->addWidget(row.parity);
            format->addWidget(new QLabel(tr("Bits:"),group)); format->addWidget(row.bits);
            format->addWidget(new QLabel(tr("Stops:"),group)); format->addWidget(row.stops); grid->addLayout(format,2,0,1,5);
        } else {
            row.address=new QLineEdit(group); row.address->setObjectName(prefix+"Address"); row.address->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
            row.port=new QSpinBox(group); row.port->setRange(0,65535); row.port->setSpecialValueText(tr("Choose port")); row.port->setObjectName(prefix+"Port"); row.port->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
            grid->addWidget(new QLabel(tr("Listen on:"),group),1,0); grid->addWidget(row.address,1,1,1,2);
            grid->addWidget(new QLabel(tr("Port:"),group),1,3); grid->addWidget(row.port,1,4);
            row.pty=new QCheckBox(tr("Enable Thetis PTY"),group); row.pty->setObjectName(prefix+"Pty"); row.pty->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle)); grid->addWidget(row.pty,2,0,1,2);
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
            row.pty->setEnabled(false); row.pty->setToolTip(tr("Native PTYs are available only on macOS and Linux; use a supplied virtual COM device here."));
#endif
            row.path=note(group,{}); row.path->setObjectName(prefix+"Path"); row.path->setTextInteractionFlags(Qt::TextSelectableByMouse); grid->addWidget(row.path,2,2,1,3);
        }
        row.status=note(group,{}); row.status->setObjectName(prefix+"Status"); grid->addWidget(row.status,3,0,1,5);
        contentLayout()->insertWidget(contentLayout()->count()-1,group);
        connect(row.enabled,&QCheckBox::toggled,this,[this,i] { apply(i+1); });
        if (row.pty) { connect(row.pty,&QCheckBox::toggled,this,[this,i] { apply(i+1); }); }
        for (QComboBox* widget:{row.primary,row.secondary,row.baud,row.parity,row.bits,row.stops}) {
            if (!widget) { continue; } widget->installEventFilter(this);
            connect(widget,&QComboBox::currentIndexChanged,this,[this,i] { apply(i+1); });
        }
        if (row.device) { row.device->installEventFilter(this); connect(row.device->lineEdit(),&QLineEdit::editingFinished,this,[this,i] { apply(i+1); }); connect(row.device,&QComboBox::activated,this,[this,i] { apply(i+1); }); }
        if (row.address) { connect(row.address,&QLineEdit::editingFinished,this,[this,i] { apply(i+1); }); }
        if (row.port) { row.port->installEventFilter(this); connect(row.port,&QSpinBox::valueChanged,this,[this,i] { apply(i+1); }); }
    }
    if (!serial) {
        auto* rigctld=new QCheckBox(tr("Hamlib rigctld"),this); rigctld->setEnabled(false); rigctld->setToolTip(tr("The separate rigctld backend has not been delivered yet.")); contentLayout()->insertWidget(contentLayout()->count()-1,rigctld);
        contentLayout()->insertWidget(contentLayout()->count()-1,note(this,tr("Each channel controls its assigned slice IDs independently of GUI focus. PTY paths are created only while the transport is open.")));
    }
    if (m_service) {
        connect(m_service,&CatService::configurationChanged,this,[this] { syncFromModel(); });
        connect(m_service,&CatService::channelStateChanged,this,[this] { syncFromModel(); });
        connect(m_service,&CatService::clientCountChanged,this,[this] { syncFromModel(); });
        connect(m_service,&CatService::ptyPathChanged,this,[this] { syncFromModel(); });
        connect(m_service,&CatService::transportStateChanged,this,[this] { syncFromModel(); });
    }
    if (model) { connect(model,&RadioModel::sliceAdded,this,[this] { syncFromModel(); }); connect(model,&RadioModel::sliceRemoved,this,[this] { syncFromModel(); }); }
    syncFromModel();
}
bool CatChannelSetupPage::eventFilter(QObject* watched,QEvent* event) {
    if (event->type()==QEvent::Wheel) { return true; } return SetupPage::eventFilter(watched,event);
}
void CatChannelSetupPage::apply(int channel) {
    if (m_syncing || !m_service || !local(model())) { return; }
    const Row& row=m_rows[channel-1]; CatEndpointConfig config=m_service->channelConfig(channel);
    config.binding.primarySliceId=row.primary->currentData().toInt(); const int secondary=row.secondary->currentData().toInt();
    config.binding.secondarySliceId=secondary<0 ? std::nullopt : std::optional<int>(secondary);
    config.binding.primaryIncarnation=row.primary->currentData(Qt::UserRole+1).toULongLong();
    config.binding.secondaryIncarnation=secondary<0 ? std::nullopt : std::optional<quint64>(row.secondary->currentData(Qt::UserRole+1).toULongLong());
    if (m_serial) { config.serialEnabled=row.enabled->isChecked(); config.serialDevice=row.device->currentText().trimmed(); config.serialBaud=row.baud->currentText().toInt(); config.serialParity=row.parity->currentText(); config.serialDataBits=row.bits->currentText().toInt(); config.serialStopBits=row.stops->currentText(); }
    else { config.tcpEnabled=row.enabled->isChecked(); config.tcpBindAddress=row.address->text().trimmed(); config.tcpPort=row.port->value(); config.ptyEnabled=row.pty->isChecked(); }
    const QPointer<CatChannelSetupPage> lifetime(this);
    const bool accepted=m_service->reconfigureChannel(channel,config);
    if (!lifetime) { return; }
    syncFromModel();
    if (!accepted) { m_rows[channel-1].status->setText(tr("Configuration refused: check address, port, format and exclusive device assignment.")); }
}
void CatChannelSetupPage::syncFromModel() {
    if (!m_service) { return; } m_syncing=true;
    for (int i=0;i<4;++i) {
        Row& row=m_rows[i]; const CatEndpointConfig config=m_service->channelConfig(i+1);
        fillSlices(row.primary,model(),config.binding.primarySliceId,config.binding.primaryIncarnation); fillSlices(row.secondary,model(),config.binding.secondarySliceId.value_or(-1),config.binding.secondaryIncarnation.value_or(0));
        { const QSignalBlocker block(row.enabled); row.enabled->setChecked(m_serial ? config.serialEnabled : config.tcpEnabled); }
        if (m_serial) { setChoice(row.device,config.serialDevice); setChoice(row.baud,QString::number(config.serialBaud)); setChoice(row.parity,config.serialParity); setChoice(row.bits,QString::number(config.serialDataBits)); setChoice(row.stops,config.serialStopBits); row.status->setText(tr("Serial: %1").arg(m_service->transportState(i+1,CatTransportKind::Serial))); }
        else {
            const QSignalBlocker address(row.address), port(row.port), pty(row.pty);
            row.address->setText(config.tcpBindAddress); row.port->setValue(config.tcpPort); row.pty->setChecked(config.ptyEnabled);
            row.path->setText(m_service->ptySlavePath(i+1).isEmpty() ? tr("PTY: %1").arg(m_service->transportState(i+1,CatTransportKind::Pty)) : m_service->ptySlavePath(i+1));
            row.status->setText(tr("TCP: %1 · Bound: %2:%3 · Clients: %4").arg(m_service->transportState(i+1,CatTransportKind::Tcp),m_service->boundAddress(i+1).toString()).arg(m_service->boundPort(i+1)).arg(m_service->clientCount(i+1)));
        }
    }
    m_syncing=false;
}
CatGlobalSetupPage::CatGlobalSetupPage(const QString& title,RadioModel* model,QWidget* parent)
    : SetupPage(title,model,parent),m_service(model ? model->catService() : nullptr) {
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));
    if (!local(model)) { contentLayout()->insertWidget(contentLayout()->count()-1,note(this,localReason())); }
    if (m_service) { connect(m_service,&CatService::globalConfigurationChanged,this,[this] { syncFromModel(); }); }
}
bool CatGlobalSetupPage::eventFilter(QObject* watched,QEvent* event) { if (event->type()==QEvent::Wheel) { return true; } return SetupPage::eventFilter(watched,event); }
QCheckBox* CatGlobalSetupPage::addCheck(QFormLayout* form,const QString& label,const QString& name,bool CatGlobalConfig::* field) {
    auto* widget=new QCheckBox(label,this); widget->setMinimumHeight(Style::kButtonH); widget->setObjectName(name); widget->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle)); widget->setEnabled(local(model())); form->addRow(widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { const QSignalBlocker blocked(widget); widget->setChecked(config.*field); });
    connect(widget,&QCheckBox::toggled,this,[this,field](bool value) { if (m_syncing || !m_service) { return; } CatGlobalConfig config=m_service->globalConfig(); config.*field=value; applyConfiguration(config); }); return widget;
}
QComboBox* CatGlobalSetupPage::addChoice(QFormLayout* form,const QString& label,const QString& name,const QStringList& items,QString CatGlobalConfig::* field) {
    auto* widget=combo(this,items,name); widget->setEnabled(local(model())); widget->installEventFilter(this); form->addRow(label,widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { setChoice(widget,config.*field); });
    connect(widget,&QComboBox::currentTextChanged,this,[this,field](const QString& value) { if (m_syncing || !m_service) { return; } CatGlobalConfig config=m_service->globalConfig(); config.*field=value; applyConfiguration(config); }); return widget;
}
QLineEdit* CatGlobalSetupPage::addText(QFormLayout* form,const QString& label,const QString& name,QString CatGlobalConfig::* field) {
    auto* widget=new QLineEdit(this); widget->setObjectName(name); widget->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle)); widget->setEnabled(local(model())); form->addRow(label,widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { const QSignalBlocker blocked(widget); widget->setText(config.*field); });
    connect(widget,&QLineEdit::editingFinished,this,[this,widget,field] { if (m_syncing || !m_service) { return; } CatGlobalConfig config=m_service->globalConfig(); config.*field=widget->text(); applyConfiguration(config); }); return widget;
}
QSpinBox* CatGlobalSetupPage::addNumber(QFormLayout* form,const QString& label,const QString& name,int low,int high,int CatGlobalConfig::* field) {
    auto* widget=new QSpinBox(this); widget->setObjectName(name); widget->setRange(low,high); widget->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle)); widget->setEnabled(local(model())); widget->installEventFilter(this); form->addRow(label,widget);
    m_updates.emplace_back([widget,field](const CatGlobalConfig& config) { const QSignalBlocker blocked(widget); widget->setValue(config.*field); });
    connect(widget,&QSpinBox::valueChanged,this,[this,field](int value) { if (m_syncing || !m_service) { return; } CatGlobalConfig config=m_service->globalConfig(); config.*field=value; applyConfiguration(config); }); return widget;
}
void CatGlobalSetupPage::applyConfiguration(const CatGlobalConfig& config) {
    if (!m_service) { return; }
    const QPointer<CatGlobalSetupPage> lifetime(this);
    const bool accepted=m_service->reconfigureGlobal(config);
    if (!lifetime) { return; }
    syncFromModel();
    if (!accepted && m_status) { m_status->setText(tr("Configuration refused: check PTT source, sampled inputs and device assignment.")); }
}
void CatGlobalSetupPage::syncFromModel() { if (!m_service) { return; } m_syncing=true; const CatGlobalConfig config=m_service->globalConfig(); for (const auto& update:m_updates) { update(config); } if (m_status) { m_status->setText(tr("PTT: %1").arg(m_service->pttState())); } m_syncing=false; }
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
    auto* recenter=new QCheckBox(tr("Always recenter VFOs"),options); recenter->setEnabled(false); recenter->setToolTip(tr("CAT recentering of panadapters is not available in this delivery.")); form->addRow(recenter);
    auto* rtty=addSection(tr("RTTY frequency reporting")); auto* offsets=sectionForm(rtty);
    addCheck(offsets,tr("Apply offset to VFO A"),"catRttyA",&CatGlobalConfig::rttyOffsetAEnabled);
    addCheck(offsets,tr("Apply offset to VFO B"),"catRttyB",&CatGlobalConfig::rttyOffsetBEnabled);
    addNumber(offsets,tr("DIGU offset (Hz):"),"catRttyDigu",CatDefaults::kRttyMinimumHz,CatDefaults::kRttyMaximumHz,&CatGlobalConfig::rttyDiguHz);
    addNumber(offsets,tr("DIGL offset (Hz):"),"catRttyDigl",CatDefaults::kRttyMinimumHz,CatDefaults::kRttyMaximumHz,&CatGlobalConfig::rttyDiglHz);
    auto* testing=addSection(tr("CAT Tester")); auto* testForm=sectionForm(testing);
    auto* channel=combo(testing,{"1","2","3","4"},"catTesterChannel"); channel->installEventFilter(this); testForm->addRow(tr("Channel:"),channel);
    auto* command=new QLineEdit("ID;",testing); command->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle)); command->setObjectName("catTesterCommand"); testForm->addRow(tr("Command:"),command);
    auto* reply=note(testing,{}); reply->setMinimumWidth(200); reply->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred); reply->setObjectName("catTesterReply"); reply->setTextInteractionFlags(Qt::TextSelectableByMouse); testForm->addRow(tr("Reply:"),reply);
    auto* send=new QPushButton(tr("Test command"),testing); send->setAutoDefault(false); send->setStyleSheet(QString::fromLatin1(Style::kButtonStyle)); send->setObjectName("catTesterSend"); send->setEnabled(local(model)); testForm->addRow(send);
    connect(send,&QPushButton::clicked,this,[this,channel,command,reply] {
        if (!m_service) { return; }
        const QPointer<CatOptionsSetupPage> lifetime(this); const QPointer<QLabel> output(reply);
        const QByteArray bytes=m_service->testCommand(channel->currentText().toInt(),command->text().toLatin1());
        if (lifetime && output) { output->setText(bytes.isEmpty() ? tr("Accepted setter: no wire reply") : QString::fromLatin1(bytes)); }
    });
    testForm->addRow(note(testing,tr("Tests use the real parser and model. RX and parameter changes take effect. TX, Tune, Two Tone, VOX enable and PureSignal single/calibration are refused.")));
    auto* log=new QPushButton(tr("Show CAT Log…"),testing); log->setObjectName("catShowLog"); log->setAutoDefault(false); log->setStyleSheet(QString::fromLatin1(Style::kButtonStyle)); log->setEnabled(local(model)); connect(log,&QPushButton::clicked,this,&CatOptionsSetupPage::showLogRequested); testForm->addRow(log);
    syncFromModel();
}
CatPttSetupPage::CatPttSetupPage(RadioModel* model,QWidget* parent) : CatGlobalSetupPage(tr("CAT PTT"),model,parent) {
    auto* group=addSection(tr("Release-armed input PTT")); auto* form=sectionForm(group);
    addCheck(form,tr("Enable input PTT"),"catPttEnabled",&CatGlobalConfig::pttEnabled);
    addChoice(form,tr("Input source:"),"catPttSource",{"None","CAT1","CAT2","CAT3","CAT4","Physical"},&CatGlobalConfig::pttDeviceSource);
    addCheck(form,tr("Legacy RTS wiring — samples CTS input"),"catPttCts",&CatGlobalConfig::pttUseCts);
    addCheck(form,tr("Legacy DTR wiring — samples DSR input"),"catPttDsr",&CatGlobalConfig::pttUseDsr);
    addText(form,tr("Separate physical device:"),"catPttDevice",&CatGlobalConfig::pttSerialDevice);
    addNumber(form,tr("Physical device's CAT channel:"),"catPttChannel",1,4,&CatGlobalConfig::pttChannel);
    // Source serial choices are the same as the ordinary CAT ports, cited above.
    auto* baud=combo(group,baudChoices(),"catPttBaud"); baud->installEventFilter(this); form->addRow(tr("Baud:"),baud); m_updates.emplace_back([baud](const CatGlobalConfig& c) { setChoice(baud,QString::number(c.pttSerialBaud)); });
    connect(baud,&QComboBox::currentTextChanged,this,[this](const QString& text) { if (!m_syncing && m_service) { CatGlobalConfig c=m_service->globalConfig(); c.pttSerialBaud=text.toInt(); applyConfiguration(c); } });
    auto* parity=addChoice(form,tr("Parity:"),"catPttParity",{"None","Odd","Even","Mark","Space"},&CatGlobalConfig::pttSerialParity);
    // Qt Data5–Data8: https://doc.qt.io/qt-6/qserialport.html#DataBits-enum
    addNumber(form,tr("Data bits:"),"catPttBits",5,8,&CatGlobalConfig::pttSerialDataBits);
    auto* stops=addChoice(form,tr("Stop bits:"),"catPttStops",{"1","1.5","2"},&CatGlobalConfig::pttSerialStopBits); serialPlatformChoices(parity,stops);
    m_status=note(group,{}); m_status->setObjectName("catPttStatus"); form->addRow(m_status);
    form->addRow(note(group,tr("CAT1–4 share an already open serial CAT device. Physical uses one exclusive device. All selected inputs must release before a fresh assertion can request TX. Opening/restoring configuration never keys. RTS/DTR outputs are not asserted by these controls.")));
#ifndef HAVE_SERIALPORT
    group->setEnabled(false); group->setToolTip(tr("QtSerialPort dependency unavailable in this build.")); contentLayout()->insertWidget(contentLayout()->count()-1,note(this,tr("Input PTT unavailable: QtSerialPort dependency is absent.")));
#endif
    if (!local(model)) { group->setEnabled(false); }
    if (m_service) { connect(m_service,&CatService::pttStateChanged,this,[this] { syncFromModel(); }); }
    syncFromModel();
}
} // namespace NereusSDR
