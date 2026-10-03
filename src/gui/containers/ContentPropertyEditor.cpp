// no-port-check: NereusSDR-original contextual draft property editor.
// Modification history (NereusSDR):
//   2026-10-03 — Plain applet source explanations by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include "ContentPropertyEditor.h"
#include "ContainerSettingsDialog.h"
#include "meter_property_editors/BaseItemEditor.h"
#include "meter_property_editors/SignalTextItemEditor.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QJsonArray>
#include <QColor>
#include <QColorDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QRegularExpression>
#include "gui/meters/MeterPoller.h"
namespace NereusSDR {
ContentPropertyEditor::ContentPropertyEditor(ContainerContentRegistry& registry,QWidget* parent):QWidget(parent),m_registry(registry) {}
void ContentPropertyEditor::setContainerDefaults(const QJsonObject& defaults)
{
    m_defaults=defaults;
    if(auto* source=findChild<QComboBox*>("contentSlice")) {
        const int inherited=defaults.contains("sliceId")?defaults["sliceId"].toInt():defaults["rxSource"].toInt(1)-1;
        source->setItemText(0,tr("Container default — Slice %1").arg(QChar('A'+inherited)));
    }
    if(auto* session=findChild<QLineEdit*>("sessionId")) {session->setPlaceholderText(tr("Container default — %1").arg(defaults["sessionId"].toString(tr("current session"))));}
}
ContentPropertyEditor::~ContentPropertyEditor()=default;
void ContentPropertyEditor::publish()
{
    if(m_loading) { return; }
    emit entryEdited(m_entry);
}
bool ContentPropertyEditor::property(const QString& key,const QJsonValue& value,int channel)
{
    QJsonObject change;
    if(channel>=0) {
        auto props=qobject_cast<CompositePresetItem*>(m_item.get())->configuration();
        auto channels=props.value("channels").toArray(); auto c=channels[channel].toObject(); c[key]=value; channels[channel]=c;
        change["channels"]=channels;
        if(channel==0 && key=="bindingId") { change["bindingId"]=value; }
    } else { change[key]=value; }
    bool accepted=false;
    if(auto* face=qobject_cast<CompositePresetItem*>(m_item.get())) { accepted=face->applyConfiguration(change); }
    if(auto* bar=qobject_cast<BarPresetItem*>(m_item.get())) { accepted=bar->applyConfiguration(change); }
    if(!accepted) { if(m_error) {m_error->setText(tr("Value rejected: check the range, history duration or color."));} return false; }
    if(m_error) {m_error->clear();}
    captureRenderer();
    publish(); return true;
}
void ContentPropertyEditor::captureRenderer()
{
    auto captured=m_registry.captureMeterItem(*m_item,m_entry);
    // Renderer normalization is independent of name, visibility and source axes.
    // Undoing a face edit restores its original sparse/raw config even when
    // another axis changed during this editor selection.
    if(captured.config==m_hydrated.config) {captured.config=m_original.config;}
    if(captured.canvasRect==m_hydrated.canvasRect) {captured.canvasRect=m_original.canvasRect;}
    for(const QString& key:{QStringLiteral("bindingId"),QStringLiteral("mmioGuid"),QStringLiteral("mmioVariable"),QStringLiteral("stackSlot"),QStringLiteral("slotLocalY"),QStringLiteral("slotLocalH"),QStringLiteral("onlyWhenRx"),QStringLiteral("onlyWhenTx"),QStringLiteral("displayGroup")}) {
        if(captured.context[key]==m_hydrated.context[key]) {
            if(m_original.context.contains(key)) {captured.context[key]=m_original.context[key];} else {captured.context.remove(key);}
        }
    }
    m_entry=captured;
}
QJsonValue ContentPropertyEditor::effectiveProperty(const QString& key,int channel) const
{
    QJsonObject config;
    if(auto* face=qobject_cast<CompositePresetItem*>(m_item.get())) {config=face->configuration();}
    if(auto* bar=qobject_cast<BarPresetItem*>(m_item.get())) {config=bar->configuration();}
    return channel<0?config[key]:config["channels"].toArray()[channel].toObject()[key];
}
namespace {
QString fieldLabel(QString key)
{
    if(key=="bindingId") {return QObject::tr("Primary reading");}
    if(key=="secondaryBindingId") {return QObject::tr("Average reading");}
    if(key=="decay") {return QObject::tr("Release (0–1)");}
    if(key=="attack") {return QObject::tr("Attack (0–1)");}
    key.replace(QRegularExpression("([a-z])([A-Z])"),"\\1 \\2");
    if(!key.isEmpty()) {key[0]=key[0].toUpper();}
    return key;
}
}
QWidget* ContentPropertyEditor::control(const QString& key,const QJsonValue& value,int channel)
{
    QWidget* widget=nullptr;
    if(key.endsWith("BindingId") || key=="bindingId") {
        auto* combo=new QComboBox(this);combo->addItem(tr("Unassigned"),-1);
        // Existing GUI binding names are the source of truth, including gaps.
        for(int id=0;id<=MeterBinding::RotatorEle;++id) {const auto title=readingName(id);if(!title.isEmpty()) {combo->addItem(title,id);}}
        if(combo->findData(value.toInt())<0) {combo->addItem(tr("Stored reading %1").arg(value.toInt()),value.toInt());}
        combo->setCurrentIndex(combo->findData(value.toInt()));
        connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,key,channel,combo]{property(key,combo->currentData().toInt(),channel);});widget=combo;
    } else if(key=="style" || key=="clockMode") {
        auto* combo=new QComboBox(this);combo->addItems(key=="style"?QStringList{"Line","Solid","Segments"}:QStringList{"Both","UTC","Local"});combo->setCurrentText(value.toString());
        connect(combo,&QComboBox::currentTextChanged,this,[this,key,channel](const QString& text){property(key,text,channel);});widget=combo;
    } else if(value.isBool()) {
        auto* check=new QCheckBox(this);check->setChecked(value.toBool());connect(check,&QCheckBox::toggled,this,[this,key,channel](bool v){property(key,v,channel);});widget=check;
    } else if(value.isDouble()) {
        const bool integral=key.endsWith("Ms") || QStringList{"rowHeight","faceHeight","fontSize","displayGroup","historyCapacity"}.contains(key);
        if(integral) {
            auto* spin=new QSpinBox(this);int low=0;
            if(key=="updateIntervalMs") {low=1;}if(key=="historyCapacity") {low=2;}if(key=="fontSize") {low=8;}
            if(key=="historyMs") {low=qobject_cast<CompositePresetItem*>(m_item.get())?100:effectiveProperty("updateIntervalMs",channel).toInt(1);}if(key=="rowHeight") {low=72;}
            if(key=="faceHeight") {low=qobject_cast<CompositePresetItem*>(m_item.get())->minimumConfiguredFaceHeight();}
            spin->setRange(low,1000000);spin->setValue(value.toInt());if(key.endsWith("Ms")) {spin->setSuffix(tr(" ms"));}if(key.endsWith("Height") || key=="fontSize") {spin->setSuffix(tr(" px"));}
            connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,[this,key,channel,spin](int v){if(!property(key,v,channel)) {const QSignalBlocker block(spin);spin->setValue(effectiveProperty(key,channel).toInt());spin->setToolTip(tr("Value rejected; the previous effective value is restored."));}});widget=spin;
        } else {
            auto* spin=new QDoubleSpinBox(this);spin->setDecimals(4);spin->setRange(-1000000,1000000);
            if(key=="attack" || key=="decay") {spin->setRange(0,1);spin->setSingleStep(.05);}
            if(key.startsWith("radius") || key=="strokeWidth" || key=="lengthFactor") {spin->setRange(.0001,1000);spin->setSingleStep(.05);}
            spin->setValue(value.toDouble());connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,key,channel,spin](double v){if(!property(key,v,channel)) {const QSignalBlocker block(spin);spin->setValue(effectiveProperty(key,channel).toDouble());spin->setToolTip(tr("Value rejected; the previous effective value is restored."));}});widget=spin;
        }
    } else if(value.isString()) {
        auto* edit=new QLineEdit(value.toString(),this);edit->setObjectName(channel<0?key:QString("channel%1_%2").arg(channel).arg(key));
        connect(edit,&QLineEdit::editingFinished,this,[this,edit,key,channel]{if(!property(key,edit->text(),channel)) {edit->setText(effectiveProperty(key,channel).toString());edit->setToolTip(tr("Value rejected; the previous effective value is restored."));}});widget=edit;
        if(key.endsWith("Color") || key=="color") {
            edit->setPlaceholderText(tr("#AARRGGBB"));auto* row=new QWidget(this);auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);layout->addWidget(edit);
            auto* choose=new QPushButton(tr("Choose…"),row);choose->setAutoDefault(false);layout->addWidget(choose);
            connect(choose,&QPushButton::clicked,this,[this,key,channel,edit]{const auto color=QColorDialog::getColor(QColor(edit->text()),this,tr("Choose color"),QColorDialog::ShowAlphaChannel);if(color.isValid() && property(key,color.name(QColor::HexArgb),channel)) {edit->setText(color.name(QColor::HexArgb));}});widget=row;
        }
    }
    if(widget) {widget->setObjectName(channel<0?key:QString("channel%1_%2").arg(channel).arg(key));}
    return widget;
}
void ContentPropertyEditor::setEntry(const ContentEntry& entry)
{
    m_loading=true;
    if(auto* old=layout()) { while(auto* child=old->takeAt(0)) { delete child->widget(); delete child; } delete old; }
    m_item.reset(); m_entry=entry; m_original=entry;
    auto* root=new QVBoxLayout(this);
    auto* common=new QGroupBox(tr("Object"),this); auto* form=new QFormLayout(common); root->addWidget(common);
    auto* name=new QLineEdit(entry.name,common); name->setObjectName("contentName"); form->addRow(tr("Name"),name);
    connect(name,&QLineEdit::textEdited,this,[this](const QString& value){m_entry.name=value;publish();});
    auto* visible=new QCheckBox(common); visible->setObjectName("contentVisible"); visible->setChecked(entry.visible); form->addRow(tr("Visible"),visible);
    connect(visible,&QCheckBox::toggled,this,[this](bool value){m_entry.visible=value;publish();});
    m_item.reset(m_registry.createMeterItem(entry,nullptr,ContentRenderMode::Preview));
    if(m_item) {m_hydrated=m_registry.captureMeterItem(*m_item,entry);}
    QString reason=entry.extensions.value("unavailableReason").toString();
    if(reason.isEmpty() && !m_registry.isAvailable(entry.typeId)) { reason=m_registry.unavailableReason(entry.typeId); }
    if(!reason.isEmpty() || (!m_item && !entry.typeId.startsWith("applet:"))) {
        auto* note=new QLabel(reason.isEmpty()?tr("Unsupported configuration. Original data is retained; this object can be removed explicitly."):reason,this); note->setWordWrap(true); root->addWidget(note);
    } else if(auto* composite=qobject_cast<CompositePresetItem*>(m_item.get())) {
        const auto props=composite->configuration();
        auto* group=new QGroupBox(tr("Appearance and behavior"),this); auto* fields=new QFormLayout(group); root->addWidget(group);
        const auto add=[this](QFormLayout* target,const QString& key,const QJsonValue& value,int channel) {
            if(key=="bindingId" || QStringList{"x","y","w","h","channels","vfo","bands","modes"}.contains(key)) {return;}
            if(auto* widget=control(key,value,channel)) {target->addRow(fieldLabel(key),widget);}
        };
        for(const auto& key:composite->editableFields()) { add(fields,key,props[key],-1); }
        const auto channels=props["channels"].toArray();
        for(int i=0;i<channels.size();++i) {
            const auto c=channels[i].toObject(); auto* group=new QGroupBox(tr("Channel %1 — %2").arg(i+1).arg(c["name"].toString()),this); auto* fields=new QFormLayout(group); root->addWidget(group);
            for(const auto& key:composite->editableChannelFields(i)) { add(fields,key,c[key],i); }
        }
        if(composite->typeId()=="meter.signalText") { auto* note=new QLabel(tr("Signal units and decimal precision follow the global multimeter preferences."),this); note->setWordWrap(true); root->addWidget(note); }
    } else if(auto* bar=qobject_cast<BarPresetItem*>(m_item.get())) {
        auto* group=new QGroupBox(tr("Appearance and behavior"),this); auto* fields=new QFormLayout(group); root->addWidget(group);
        const auto props=bar->configuration();
        QStringList keys{"label","barColor","backdropColor","titleColor","historyColor","showHistory","peakHold","showValue","showPeakValue","style","updateIntervalMs","historyMs","ignoreHistoryMs","rowHeight","attack","decay","bindingId","secondaryBindingId"};
        const QString flavor=props["flavor"].toString();
        if(flavor=="Custom") { keys.append({"minValue","maxValue","redThreshold","units"}); }
        keys.removeAll("bindingId");keys.removeAll("secondaryBindingId");
        for(const auto& key:keys) {if(auto* widget=control(key,props[key])) {fields->addRow(fieldLabel(key),widget);}}
        auto* note=new QLabel(flavor=="Custom"?tr("Custom calibration range is editable."):tr("This face uses its calibrated source scale; minimum and maximum are fixed."),this); note->setWordWrap(true); root->addWidget(note);
        if(flavor.contains("Signal") || flavor=="PbSnr") { auto* note=new QLabel(tr("Signal units and decimal precision follow the global multimeter preferences."),this); note->setWordWrap(true); root->addWidget(note); }
    } else if(m_item) {
        if(auto* adapter=ContainerSettingsDialog::buildTypeSpecificEditor(m_item.get(),this)) {
            root->addWidget(adapter);
            if(auto* signal=qobject_cast<SignalTextItemEditor*>(adapter)) {signal->useGlobalUnits();}
            qobject_cast<BaseItemEditor*>(adapter)->setGeometryEditable(m_policy==ContentLayout::LegacyCanvas);
            connect(qobject_cast<BaseItemEditor*>(adapter),&BaseItemEditor::propertyChanged,this,[this]{
                captureRenderer(); publish();
            });
        }
    } else {
        auto* note=new QLabel(tr("Existing applet view. Changes are staged until Apply; its preview controls are disabled."),this); note->setWordWrap(true); root->addWidget(note);
    }
    auto* advanced=new QGroupBox(tr("Advanced bindings"),this); auto* bindings=new QFormLayout(advanced); root->addWidget(advanced);
    const auto* composite=qobject_cast<CompositePresetItem*>(m_item.get());
    const auto* bar=qobject_cast<BarPresetItem*>(m_item.get());
    if(composite || bar) {
        const auto props=composite?composite->configuration():bar->configuration();
        for(const auto& key:{QStringLiteral("bindingId"),QStringLiteral("secondaryBindingId")}) {if(bar && props.contains(key)) {bindings->addRow(fieldLabel(key),control(key,props[key]));}}
        if(composite) {const auto channels=props["channels"].toArray();for(int i=0;i<channels.size();++i) {bindings->addRow(tr("%1 reading").arg(channels[i].toObject()["name"].toString(tr("Channel %1").arg(i+1))),control("bindingId",channels[i].toObject()["bindingId"],i));}}
    }
    const bool applet=entry.typeId.startsWith("applet:");
    if((composite && composite->typeId()=="meter.clock") || entry.typeId=="CLOCK") {
        advanced->setEnabled(false);advanced->setToolTip(tr("This clock uses local time independently of radio and MMIO sources. Stored context is retained."));
    } else if(applet && entry.typeId!="applet:s_meter") {
        advanced->setEnabled(false);advanced->setToolTip(tr("This applet keeps its current radio and slice. Saved source choices are retained."));
    } else if(!m_item && !applet) {advanced->setEnabled(false);advanced->setToolTip(tr("Unsupported content retains its stored bindings."));}
    auto* source=new QComboBox(advanced); source->setObjectName("contentSlice");
    const int inherited=m_defaults.contains("sliceId")?m_defaults["sliceId"].toInt():m_defaults["rxSource"].toInt(1)-1;
    source->addItem(tr("Container default — Slice %1").arg(QChar('A'+inherited)),-1);
    for(int i=0;i<4;++i) { source->addItem(tr("Slice %1").arg(QChar('A'+i)),i); }
    const int selected=entry.context.contains("sliceId")?entry.context["sliceId"].toInt():entry.context.contains("rxSource")?entry.context["rxSource"].toInt()-1:-1;
    source->setCurrentIndex(source->findData(selected)); bindings->addRow(tr("Source"),source);
    connect(source,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,source]{
        m_entry.context.remove("rxSource"); m_entry.context.remove("sliceId");
        if(source->currentData().toInt()>=0) {m_entry.context["sliceId"]=source->currentData().toInt();} publish();
    });
    for(const QString& key:{QStringLiteral("sessionId"),QStringLiteral("mmioGuid"),QStringLiteral("mmioVariable")}) {
        if(applet && key!="sessionId") {continue;}
        auto* edit=new QLineEdit(entry.context[key].toString(),advanced); edit->setObjectName(key);
        edit->setPlaceholderText(key=="sessionId"?tr("Container default — %1").arg(m_defaults[key].toString(tr("current session"))):tr("Unassigned"));
        bindings->addRow(key=="sessionId"?tr("Radio session"):key=="mmioGuid"?tr("MMIO source UUID"):tr("MMIO variable"),edit);
        connect(edit,&QLineEdit::editingFinished,this,[this,edit,key]{ if(edit->text().isEmpty()) {m_entry.context.remove(key);} else {m_entry.context[key]=edit->text();}
            if(m_item && (key=="mmioGuid" || key=="mmioVariable")) {m_item->setMmioBinding(QUuid(m_entry.context["mmioGuid"].toString()),m_entry.context["mmioVariable"].toString());}
            publish(); });
    }
    m_error=new QLabel(this);m_error->setWordWrap(true);m_error->setStyleSheet("color:#ffb0b0");root->addWidget(m_error);
    for(auto* group:findChildren<QGroupBox*>()) {if(auto* form=qobject_cast<QFormLayout*>(group->layout())) {form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setFormAlignment(Qt::AlignLeft|Qt::AlignTop);form->setLabelAlignment(Qt::AlignLeft);}}
    root->addStretch(); m_loading=false;
}
}
