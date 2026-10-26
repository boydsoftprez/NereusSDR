// no-port-check: NereusSDR-original contextual draft property editor.
// Modification history (NereusSDR):
//   2026-10-26 - Clock: a "Thetis time and date format" row (thetisFormat).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Signal history: a "Trace 2 source" row (Off, Slice A-D,
//                 Follow selected RX) writes the face's trace2Context.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Power limit row for the PWR and Reverse PWR bars.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Bar preset rows for the Thetis colour set: show title,
//                 show marker, peak hold and peak value colours, average
//                 marker show and colour, solid and segment low and high
//                 colours, scale low and high colours. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Signal text rows: Show type, Type color, Show µV and dBm
//                 and µV and dBm color. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Magic eye rows: eye size and bezel size (0.01 to 1) and a
//                 bezel image with a file picker. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Rows for the ANAN, Cross and Power/SWR per-item options:
//                 power limit (0 shows "Radio rating"), power scale, peak
//                 hold, peak value and other needles colors (blank keeps
//                 the face's ink), shadow and dark mode. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - One data source row pair per variable the item type reads
//                 (MeterItem::mmioVariableCount, Thetis MeterVariables):
//                 variables 2 and up are stored in mmioVariables.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Fade on RX / Fade on TX rows for bar presets, composites
//                 and waveform faces (addFadeRows); a composite's edit turns
//                 its legacy fadeRx / fadeTx off. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - An RX section item's Advanced bindings are disabled with
//                 the reason, as a TX section row's are: its factory never
//                 reads the entry's context. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 - An Order control (a TX section row's parts, with Move up
//                 and Move down), and a TX section row's Advanced bindings
//                 stay disabled with the reason. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 - The MMIO source row also takes a data source's
//                 4-character ID and stores that source's UUID. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - A registered widget type with no declared properties (the
//                 TX and RX section rows) is no longer labelled an
//                 unsupported configuration; it gets the applet's binding
//                 note. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: the declared-property group is
//                 built only when it has rows (named declaredProperties), and
//                 Advanced bindings are disabled while a declared type is
//                 unavailable. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: controls for the properties a
//                 widget type declares (ContentProperty), written to the
//                 entry config; Longpath instruments get a real editor.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-04 — Selected RX source identity and RX-only presentation reset by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Plain applet source explanations by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-20 - Integer and Color controls for declared content properties.
//                 Brian Sutton, AI-assisted via xAI Grok.
#include "ContentPropertyEditor.h"
#include "ContainerSettingsDialog.h"
#include "meter_property_editors/BaseItemEditor.h"
#include "meter_property_editors/SignalTextItemEditor.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/WaveformMeterItem.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/widgets/GuardedComboBox.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/applets/SectionParts.h"
#include <QFormLayout>
#include <algorithm>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QJsonArray>
#include <QColor>
#include <QColorDialog>
#include <QFileDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <limits>
#include "gui/meters/MeterPoller.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
namespace NereusSDR {
ContentPropertyEditor::ContentPropertyEditor(ContainerContentRegistry& registry,QWidget* parent):QWidget(parent),m_registry(registry) {}
void ContentPropertyEditor::setContainerDefaults(const QJsonObject& defaults)
{
    m_defaults=defaults;
    if(auto* source=findChild<QComboBox*>("contentSlice")) {
        const int inherited=defaults.contains("sliceId")?defaults["sliceId"].toInt():defaults["rxSource"].toInt(1)-1;
        source->setItemText(0,defaults.value("rxSourceMode").toString()==QStringLiteral("followSelectedRx")
            ? tr("Container default — Follow selected RX") : tr("Container default — Slice %1").arg(QChar('A'+inherited)));
    }
    if(auto* session=findChild<QLineEdit*>("sessionId")) {session->setPlaceholderText(tr("Container default — %1").arg(defaults["sessionId"].toString(tr("current session"))));}
}
void ContentPropertyEditor::setGeometryLocked(bool locked)
{
    m_geometryLocked=locked;if(auto* group=findChild<QGroupBox*>("canvasGeometry")) {group->setEnabled(!locked);}
}
void ContentPropertyEditor::updateFreeCanvasRect(const QString& id,const QRectF& rect)
{
    if(m_entry.id!=id) {return;}m_resolvedRect=rect;m_entry.setFreeCanvasRect(rect);
    const double values[]{rect.x(),rect.y(),rect.width(),rect.height()};
    const QStringList keys{"canvasX","canvasY","canvasWidth","canvasHeight"};
    for(int i=0;i<4;++i) {if(auto* field=findChild<QDoubleSpinBox*>(keys[i])) {const QSignalBlocker blocker(field);field->setValue(values[i]);}}
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
    if (auto* waveform = qobject_cast<WaveformMeterItem*>(m_item.get())) {
        accepted = waveform->applyConfiguration(change);
    }
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
    for(const QString& key:{QStringLiteral("bindingId"),QStringLiteral("mmioGuid"),QStringLiteral("mmioVariable"),QStringLiteral("mmioVariables"),QStringLiteral("stackSlot"),QStringLiteral("slotLocalY"),QStringLiteral("slotLocalH"),QStringLiteral("onlyWhenRx"),QStringLiteral("onlyWhenTx"),QStringLiteral("displayGroup")}) {
        if(captured.context[key]==m_hydrated.context[key]) {
            if(m_original.context.contains(key)) {captured.context[key]=m_original.context[key];} else {captured.context.remove(key);}
        }
    }
    m_entry=captured;
}
void ContentPropertyEditor::addFadeRows(QFormLayout* fields)
{
    if(!m_item || !fields) {return;}
    auto* composite=qobject_cast<CompositePresetItem*>(m_item.get());
    // A composite without the entry keys shows its legacy configuration.
    const bool legacy=composite && !m_item->hasExplicitFade();
    const QJsonObject config=composite?composite->configuration():QJsonObject{};
    const bool rx=legacy?config.value("fadeRx").toBool():m_item->fadeOnRx();
    const bool tx=legacy?config.value("fadeTx").toBool():m_item->fadeOnTx();
    const auto row=[this,fields](const QString& name,const QString& label,bool checked,bool receive) {
        auto* check=new QCheckBox(fields->parentWidget());check->setObjectName(name);check->setChecked(checked);
        fields->addRow(label,check);
        connect(check,&QCheckBox::toggled,this,[this,receive](bool on) {
            if(!m_item) {return;}
            auto* face=qobject_cast<CompositePresetItem*>(m_item.get());
            const bool explicitFade=m_item->hasExplicitFade();
            const QJsonObject faceConfig=face?face->configuration():QJsonObject{};
            // Both flags are set, so the entry saves both keys.
            const bool otherRx=face && !explicitFade?faceConfig.value("fadeRx").toBool():m_item->fadeOnRx();
            const bool otherTx=face && !explicitFade?faceConfig.value("fadeTx").toBool():m_item->fadeOnTx();
            m_item->setFadeOnRx(receive?on:otherRx);
            m_item->setFadeOnTx(receive?otherTx:on);
            // From now on the composite uses the animated veil, not its
            // instant 25% fade.
            if(face && !face->applyConfiguration(QJsonObject{{"fadeRx",false},{"fadeTx",false}})) {return;}
            captureRenderer();publish();
        });
    };
    row(QStringLiteral("fadeOnRx"),tr("Fade on RX"),rx,true);
    row(QStringLiteral("fadeOnTx"),tr("Fade on TX"),tx,false);
}
QJsonValue ContentPropertyEditor::effectiveProperty(const QString& key,int channel) const
{
    QJsonObject config;
    if(auto* face=qobject_cast<CompositePresetItem*>(m_item.get())) {config=face->configuration();}
    if(auto* bar=qobject_cast<BarPresetItem*>(m_item.get())) {config=bar->configuration();}
    if (auto* waveform = qobject_cast<WaveformMeterItem*>(m_item.get())) {
        config = waveform->configuration();
    }
    return channel<0?config[key]:config["channels"].toArray()[channel].toObject()[key];
}
namespace {
QString fieldLabel(QString key)
{
    if (key == "viewWindowMs") {
        return QObject::tr("Time span");
    }
    if (key == "sourceStage") {
        return QObject::tr("Microphone stage");
    }
    if(key=="bindingId") {return QObject::tr("Primary reading");}
    if(key=="secondaryBindingId") {return QObject::tr("Average reading");}
    if(key=="decay") {return QObject::tr("Release (0–1)");}
    if(key=="attack") {return QObject::tr("Attack (0–1)");}
    if(key=="powerLimitW") {return QObject::tr("Power limit");}
    if(key=="powerScaleColor") {return QObject::tr("Power scale color");}
    if(key=="peakHoldColor") {return QObject::tr("Peak hold color");}
    if(key=="peakValueColor") {return QObject::tr("Peak value color");}
    if(key=="subNeedleColor") {return QObject::tr("Other needles color");}
    if(key=="darkMode") {return QObject::tr("Dark mode");}
    if(key=="eyeScale") {return QObject::tr("Eye size");}
    if(key=="bezelScale") {return QObject::tr("Bezel size");}
    if(key=="bezelImage") {return QObject::tr("Bezel image");}
    if(key=="thetisFormat") {return QObject::tr("Thetis time and date format");}
    if(key=="showType") {return QObject::tr("Show type");}
    if(key=="typeColor") {return QObject::tr("Type color");}
    if(key=="showSubMarker") {return QObject::tr("Show µV and dBm");}
    if(key=="subMarkerColor") {return QObject::tr("µV and dBm color");}
    if(key=="showMarker") {return QObject::tr("Show marker");}
    if(key=="showSub") {return QObject::tr("Show average marker");}
    if(key=="subColor") {return QObject::tr("Average marker color");}
    if(key=="segLowColor") {return QObject::tr("Solid and segment low color");}
    if(key=="segHighColor") {return QObject::tr("Solid and segment high color");}
    key.replace(QRegularExpression("([a-z])([A-Z])"),"\\1 \\2");
    if(!key.isEmpty()) {key[0]=key[0].toUpper();}
    return key;
}
}
namespace {
QJsonValue declaredValue(const ContentProperty& property, const QJsonObject& config)
{
    if(config.contains(property.key)) {return config.value(property.key);}
    if(property.effectiveDefault) {return property.effectiveDefault(config);}
    return property.defaultValue;
}
}
namespace {
// An Order property's items: each choice's id and shared line, in built
// order (ContentProperty::Kind::Order).
QVector<QPair<QString,int>> orderItems(const ContentProperty& p)
{
    QVector<QPair<QString,int>> items;
    for(const auto& choice:p.choices) {
        const QJsonObject item=choice.value.toObject();
        items.append({item.value(QStringLiteral("id")).toString(),item.value(QStringLiteral("line")).toInt(-1)});
    }
    return items;
}

// Moves the item at `row` one place up (step -1) or down (step +1). An item
// on a shared line swaps with its neighbour on the same line; otherwise its
// whole unit (the line, or the item alone) swaps with the next unit.
QStringList movedOrder(const QStringList& order,const QHash<QString,int>& lineOf,int row,int step)
{
    if(row<0 || row>=order.size()) {return order;}
    const int line=lineOf.value(order.at(row),-1);
    const int neighbour=row+step;
    QStringList result=order;
    if(line>=0 && neighbour>=0 && neighbour<order.size() && lineOf.value(order.at(neighbour),-1)==line) {
        result.swapItemsAt(row,neighbour);
        return result;
    }
    // The units, each a run of rows: one shared line, or one item.
    QVector<QStringList> units;
    int unitOfRow=-1;
    for(int i=0;i<order.size();++i) {
        const int itemLine=lineOf.value(order.at(i),-1);
        const bool joins=itemLine>=0 && i>0 && lineOf.value(order.at(i-1),-1)==itemLine;
        if(!joins) {units.append(QStringList{});}
        units.last().append(order.at(i));
        if(i==row) {unitOfRow=units.size()-1;}
    }
    const int other=unitOfRow+step;
    if(other<0 || other>=units.size()) {return order;}
    units.swapItemsAt(unitOfRow,other);
    result.clear();
    for(const auto& unit:units) {result.append(unit);}
    return result;
}
}
QWidget* ContentPropertyEditor::declaredControl(const ContentProperty& p, QWidget* parent)
{
    const QJsonValue value=declaredValue(p,m_entry.config);
    QWidget* widget=nullptr;
    if(p.kind==ContentProperty::Kind::Order) {
        const QVector<QPair<QString,int>> items=orderItems(p);
        QStringList built;
        QHash<QString,int> lineOf;
        QHash<QString,QString> labelOf;
        for(int i=0;i<items.size();++i) {
            built.append(items.at(i).first);
            lineOf.insert(items.at(i).first,items.at(i).second);
            labelOf.insert(items.at(i).first,p.choices.at(i).label);
        }
        const QString tip=tr("Controls on one line stay together.");
        auto* box=new QWidget(parent);
        box->setToolTip(tip);
        auto* layout=new QHBoxLayout(box);
        layout->setContentsMargins(0,0,0,0);
        auto* list=new QListWidget(box);
        list->setObjectName(QStringLiteral("property_")+p.key+QStringLiteral("List"));
        list->setToolTip(tip);
        layout->addWidget(list,1);
        auto* buttons=new QVBoxLayout;
        auto* up=new QPushButton(tr("Move up"),box);
        up->setObjectName(QStringLiteral("property_")+p.key+QStringLiteral("Up"));
        up->setAutoDefault(false);
        auto* down=new QPushButton(tr("Move down"),box);
        down->setObjectName(QStringLiteral("property_")+p.key+QStringLiteral("Down"));
        down->setAutoDefault(false);
        buttons->addWidget(up);
        buttons->addWidget(down);
        buttons->addStretch();
        layout->addLayout(buttons);
        const auto fill=[list,labelOf](const QStringList& order,const QString& selected){
            const QSignalBlocker block(list);
            list->clear();
            for(const QString& id:order) {
                auto* item=new QListWidgetItem(labelOf.value(id,id),list);
                item->setData(Qt::UserRole,id);
                if(id==selected) {list->setCurrentItem(item);}
            }
        };
        fill(SectionParts::effectiveOrder(items,value.toArray()),QString());
        const auto move=[this,list,fill,lineOf,built,key=p.key](int step){
            QStringList order;
            for(int i=0;i<list->count();++i) {order.append(list->item(i)->data(Qt::UserRole).toString());}
            const int row=list->currentRow();
            if(row<0) {return;}
            const QString selected=order.at(row);
            const QStringList moved=movedOrder(order,lineOf,row,step);
            if(moved==order) {return;}
            fill(moved,selected);
            // The built order is not written.
            if(moved==built) {m_entry.config.remove(key);} else {m_entry.config[key]=QJsonArray::fromStringList(moved);}
            publish();
        };
        connect(up,&QPushButton::clicked,this,[move]{move(-1);});
        connect(down,&QPushButton::clicked,this,[move]{move(+1);});
        widget=box;
    } else if(p.kind==ContentProperty::Kind::Toggle) {
        auto* check=new QCheckBox(parent);check->setChecked(value.toBool());
        connect(check,&QCheckBox::toggled,this,[this,key=p.key](bool on){m_entry.config[key]=on;publish();});
        widget=check;
    } else if(p.kind==ContentProperty::Kind::Text) {
        auto* edit=new QLineEdit(value.toString(),parent);edit->setPlaceholderText(p.placeholder);
        connect(edit,&QLineEdit::editingFinished,this,[this,edit,key=p.key]{
            if(edit->text().isEmpty()) {m_entry.config.remove(key);} else {m_entry.config[key]=edit->text();}
            publish();
        });
        widget=edit;
    } else if(p.kind==ContentProperty::Kind::Integer) {
        auto* spin=new QSpinBox(parent);
        const int low=p.minimum;
        const int high=qMax(low, p.maximum);
        spin->setRange(low, high);
        spin->setValue(value.toInt(p.defaultValue.toInt(low)));
        if(!p.suffix.isEmpty()) {spin->setSuffix(p.suffix);}
        connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,[this,key=p.key](int chosen){
            m_entry.config[key]=chosen; publish();
        });
        widget=spin;
    } else if(p.kind==ContentProperty::Kind::Color) {
        auto* choose=new QPushButton(parent);
        choose->setAutoDefault(false);
        QString hex=value.toString();
        if(hex.isEmpty()) {hex=p.defaultValue.toString();}
        const QColor parsed(hex);
        hex=parsed.isValid()?parsed.name(QColor::HexRgb):QString::fromLatin1(Workspace::kAmberText);
        choose->setText(hex);
        connect(choose,&QPushButton::clicked,this,[this,choose,key=p.key,label=p.label]{
            const QColor picked=QColorDialog::getColor(QColor(choose->text()),this,label);
            if(!picked.isValid()) {return;}
            const QString chosen=picked.name(QColor::HexRgb);
            choose->setText(chosen);
            m_entry.config[key]=chosen;
            publish();
        });
        widget=choose;
    } else {
        auto* combo=new QComboBox(parent);
        for(const auto& choice:p.choices) {
            combo->addItem(choice.label,choice.value.toVariant());
            if(!choice.enabled) {
                if(auto* model=qobject_cast<QStandardItemModel*>(combo->model())) {
                    QStandardItem* item=model->item(combo->count()-1);
                    item->setEnabled(false);item->setToolTip(choice.disabledReason);
                }
            }
        }
        int current=-1;
        for(int i=0;i<p.choices.size();++i) {if(p.choices.at(i).value==value) {current=i;}}
        if(current<0 && !value.isNull() && !value.isUndefined()) {
            combo->addItem(tr("Stored value %1").arg(value.toVariant().toString()),value.toVariant());
            current=combo->count()-1;
        }
        combo->setCurrentIndex(current);
        connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,combo,key=p.key]{
            const QJsonValue chosen=QJsonValue::fromVariant(combo->currentData());
            if(chosen.isNull() || chosen.isUndefined()) {m_entry.config.remove(key);} else {m_entry.config[key]=chosen;}
            publish();
        });
        widget=combo;
    }
    widget->setObjectName(QStringLiteral("property_")+p.key);
    return widget;
}
QWidget* ContentPropertyEditor::control(const QString& key,const QJsonValue& value,int channel)
{
    QWidget* widget=nullptr;
    if(key=="viewWindowMs" || key=="sourceStage") {
        auto* combo=new GuardedComboBox(this);
        if(key=="viewWindowMs") {combo->addItem(tr("5 seconds"),5000);combo->addItem(tr("1 second"),1000);combo->addItem(tr("100 milliseconds"),100);}
        else {combo->addItem(tr("Conditioned encoder input"),"Conditioned");combo->addItem(tr("Raw microphone"),"RawMic");}
        combo->setCurrentIndex(combo->findData(value.toVariant()));
        connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,key,combo]{if(!property(key,QJsonValue::fromVariant(combo->currentData()))) {const QSignalBlocker blocker(combo);combo->setCurrentIndex(combo->findData(effectiveProperty(key,-1).toVariant()));}});widget=combo;
    } else if(key.endsWith("BindingId") || key=="bindingId") {
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
            if(key=="faceHeight") {low=qobject_cast<WaveformMeterItem*>(m_item.get())?180:qobject_cast<CompositePresetItem*>(m_item.get())->minimumConfiguredFaceHeight();}
            spin->setRange(low,1000000);spin->setValue(value.toInt());if(key.endsWith("Ms")) {spin->setSuffix(tr(" ms"));}if(key.endsWith("Height") || key=="fontSize") {spin->setSuffix(tr(" px"));}
            connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,[this,key,channel,spin](int v){if(!property(key,v,channel)) {const QSignalBlocker block(spin);spin->setValue(effectiveProperty(key,channel).toInt());spin->setToolTip(tr("Value rejected; the previous effective value is restored."));}});widget=spin;
        } else {
            auto* spin=new QDoubleSpinBox(this);spin->setDecimals(4);spin->setRange(-1000000,1000000);
            if(key=="attack" || key=="decay") {spin->setRange(0,1);spin->setSingleStep(.05);}
            if(key.startsWith("radius") || key=="strokeWidth" || key=="lengthFactor") {spin->setRange(.0001,1000);spin->setSingleStep(.05);}
            // Magic eye and bezel size: 1 fills the eye box.
            if(key=="eyeScale" || key=="bezelScale") {spin->setDecimals(2);spin->setRange(MagicEyeItem::kMinEyeScale,MagicEyeItem::kMaxEyeScale);spin->setSingleStep(.05);}
            // 0 follows the radio's rating; the face rejects 0-0.1.
            if(key=="powerLimitW") {spin->setDecimals(1);spin->setRange(0,kMaxBarPowerLimitW);spin->setSingleStep(10);spin->setSuffix(tr(" W"));spin->setSpecialValueText(tr("Radio rating"));}
            spin->setValue(value.toDouble());connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,key,channel,spin](double v){if(!property(key,v,channel)) {const QSignalBlocker block(spin);spin->setValue(effectiveProperty(key,channel).toDouble());spin->setToolTip(tr("Value rejected; the previous effective value is restored."));}});widget=spin;
        }
    } else if(value.isString()) {
        auto* edit=new QLineEdit(value.toString(),this);edit->setObjectName(channel<0?key:QString("channel%1_%2").arg(channel).arg(key));
        connect(edit,&QLineEdit::editingFinished,this,[this,edit,key,channel]{if(!property(key,edit->text(),channel)) {edit->setText(effectiveProperty(key,channel).toString());edit->setToolTip(tr("Value rejected; the previous effective value is restored."));}});widget=edit;
        if(key.endsWith("Color") || key=="color") {
            edit->setPlaceholderText(QStringList{"powerScaleColor","peakHoldColor","peakValueColor","segLowColor","segHighColor"}.contains(key)?tr("Blank uses the face color")
                :key=="subNeedleColor"?tr("Blank keeps each needle's color"):tr("#AARRGGBB"));auto* row=new QWidget(this);auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);layout->addWidget(edit);
            auto* choose=new QPushButton(tr("Choose…"),row);choose->setAutoDefault(false);layout->addWidget(choose);
            connect(choose,&QPushButton::clicked,this,[this,key,channel,edit]{const auto color=QColorDialog::getColor(QColor(edit->text()),this,tr("Choose color"),QColorDialog::ShowAlphaChannel);if(color.isValid() && property(key,color.name(QColor::HexArgb),channel)) {edit->setText(color.name(QColor::HexArgb));}});widget=row;
        }
        if(key=="bezelImage") {
            // A bezel is a file the user picks (or a bundled resource); blank draws none.
            edit->setPlaceholderText(tr("Blank draws no bezel"));auto* row=new QWidget(this);auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);layout->addWidget(edit);
            auto* browse=new QPushButton(tr("Browse…"),row);browse->setAutoDefault(false);layout->addWidget(browse);
            connect(browse,&QPushButton::clicked,this,[this,key,channel,edit]{const QString path=QFileDialog::getOpenFileName(this,tr("Choose bezel image"),QString(),tr("Images (*.png *.jpg *.bmp *.svg)"));if(!path.isEmpty() && property(key,path,channel)) {edit->setText(path);}});widget=row;
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
    if(m_policy==ContentLayout::FreeCanvas) {
        auto* group=new QGroupBox(tr("Canvas position and size — logical pixels"),this);group->setObjectName("canvasGeometry");group->setEnabled(!m_geometryLocked);
        auto* fields=new QFormLayout(group);root->addWidget(group);
        const QRectF rect=entry.freeCanvasRect().value_or(m_resolvedRect);
        const double values[]{rect.x(),rect.y(),rect.width(),rect.height()};
        const QStringList keys{"canvasX","canvasY","canvasWidth","canvasHeight"},labels{tr("X"),tr("Y"),tr("Width"),tr("Height")};
        QSize minimum=m_editMinimum.toSize().expandedTo(QSize(24,24));
        if(auto* face=qobject_cast<CompositePresetItem*>(m_item.get())) {minimum=face->minimumFaceSize();}
        if(auto* face=qobject_cast<BarPresetItem*>(m_item.get())) {minimum=face->minimumFaceSize();}
        if(auto* face=qobject_cast<WaveformMeterItem*>(m_item.get())) {minimum=face->minimumFaceSize();}
        for(int axis=0;axis<4;++axis) {
            auto* field=new QDoubleSpinBox(group);field->setObjectName(keys[axis]);field->setDecimals(13);field->setRange(-QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);field->setValue(values[axis]);field->setKeyboardTracking(false);fields->addRow(labels[axis],field);
            connect(field,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,axis,minimum](double value){
                if(m_geometryLocked || m_loading) {return;}
                QRectF geometry=m_entry.freeCanvasRect().value_or(m_resolvedRect);
                if(axis==0) {geometry.moveLeft(value);}if(axis==1) {geometry.moveTop(value);}
                if(axis==2) {geometry.setWidth(qMax(double(minimum.width()),value));}if(axis==3) {geometry.setHeight(qMax(double(minimum.height()),value));}
                updateFreeCanvasRect(m_entry.id,geometry);publish();
            });
        }
        auto* layer=new QSpinBox(group);layer->setObjectName("canvasLayer");layer->setRange(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());layer->setValue(entry.paintOrder);fields->addRow(tr("Layer — larger is in front"),layer);
        connect(layer,qOverload<int>(&QSpinBox::valueChanged),this,[this](int value){if(!m_geometryLocked) {m_entry.paintOrder=value;if(m_item) {m_item->setZOrder(value);}publish();}});
        auto* note=new QLabel(tr("The viewport scrolls when it is smaller than the objects. Resizing the window keeps saved object positions and sizes."),group);note->setWordWrap(true);fields->addRow(note);
    }
    QString reason=entry.extensions.value("unavailableReason").toString();
    if(reason.isEmpty() && !m_registry.isAvailable(entry.typeId)) { reason=m_registry.unavailableReason(entry.typeId); }
    // Workspace preview: a widget type with declared properties (no MeterItem).
    const QVector<ContentProperty> schema=m_item?QVector<ContentProperty>{}:m_registry.declaredProperties(entry.typeId);
    const bool declared=!schema.isEmpty();
    // A registered widget type that declares no properties (the TX and RX
    // section rows) is known and supported: it simply has no item-specific
    // options. Only a type the registry does not know is unsupported.
    const bool widgetType=!m_item && m_registry.hasWidgetFactory(entry.typeId);
    if(!reason.isEmpty() || (!m_item && !entry.typeId.startsWith("applet:") && !declared && !widgetType)) {
        auto* note=new QLabel(reason.isEmpty()?tr("Unsupported configuration. Original data is retained; this object can be removed explicitly."):reason,this); note->setWordWrap(true); root->addWidget(note);
    } else if(declared || widgetType) {
        // The group exists only when it has rows, so nothing is built and
        // thrown away.
        const bool hasRows=std::any_of(schema.cbegin(),schema.cend(),
            [](const ContentProperty& p){return p.kind!=ContentProperty::Kind::Reading;});
        if(hasRows) {
            auto* group=new QGroupBox(tr("Appearance and behavior"),this); group->setObjectName("declaredProperties");
            auto* fields=new QFormLayout(group); root->addWidget(group);
            for(const auto& p:schema) {
                if(p.kind!=ContentProperty::Kind::Reading) {fields->addRow(p.label,declaredControl(p,group));}
            }
        }
    } else if (auto* waveform = qobject_cast<WaveformMeterItem*>(m_item.get())) {
        auto* group = new QGroupBox(tr("Appearance and waveform"), this);
        auto* fields = new QFormLayout(group);
        root->addWidget(group);
        const auto props = waveform->configuration();
        QStringList keys{"title", "showTitle", "viewWindowMs", "faceHeight", "traceColor"};
        if (waveform->waveformKind() != WaveformKind::DecodedSpeech) {
            keys.append("sourceStage");
        }
        for (const auto& key : keys) {
            fields->addRow(fieldLabel(key), control(key, props[key]));
        }
        addFadeRows(fields);
        auto* note = new QLabel(tr("Fixed amplitude −1 to +1. The waveform observes existing audio; "
                                  "microphone setup is available through Raw microphone."), this);
        note->setWordWrap(true);
        root->addWidget(note);
    } else if(auto* composite=qobject_cast<CompositePresetItem*>(m_item.get())) {
        const auto props=composite->configuration();
        auto* group=new QGroupBox(tr("Appearance and behavior"),this); auto* fields=new QFormLayout(group); root->addWidget(group);
        const auto add=[this](QFormLayout* target,const QString& key,const QJsonValue& value,int channel) {
            // fadeRx / fadeTx: the Fade on RX / TX rows (addFadeRows).
            if(key=="bindingId" || QStringList{"x","y","w","h","channels","vfo","bands","modes","fadeRx","fadeTx"}.contains(key)) {return;}
            if(auto* widget=control(key,value,channel)) {target->addRow(fieldLabel(key),widget);}
        };
        for(const auto& key:composite->editableFields()) { add(fields,key,props[key],-1); }
        addFadeRows(fields);
        const auto channels=props["channels"].toArray();
        for(int i=0;i<channels.size();++i) {
            const auto c=channels[i].toObject(); auto* group=new QGroupBox(tr("Channel %1 — %2").arg(i+1).arg(c["name"].toString()),this); auto* fields=new QFormLayout(group); root->addWidget(group);
            for(const auto& key:composite->editableChannelFields(i)) { add(fields,key,c[key],i); }
        }
        if(composite->typeId()=="meter.signalText") { auto* note=new QLabel(tr("Signal units and decimal precision follow the global multimeter preferences."),this); note->setWordWrap(true); root->addWidget(note); }
    } else if(auto* bar=qobject_cast<BarPresetItem*>(m_item.get())) {
        auto* group=new QGroupBox(tr("Appearance and behavior"),this); auto* fields=new QFormLayout(group); root->addWidget(group);
        const auto props=bar->configuration();
        QStringList keys{"label","showTitle","barColor","showMarker","backdropColor","titleColor","historyColor","showHistory","peakHold","peakHoldColor","showValue","showPeakValue","peakValueColor","showSub","subColor","style","segLowColor","segHighColor","lowColor","highColor","updateIntervalMs","historyMs","ignoreHistoryMs","rowHeight","attack","decay","bindingId","secondaryBindingId"};
        const QString flavor=props["flavor"].toString();
        if(flavor=="Custom") { keys.append({"minValue","maxValue","redThreshold","units"}); }
        if(props.contains("powerLimitW")) { keys.append("powerLimitW"); }
        keys.removeAll("bindingId");keys.removeAll("secondaryBindingId");
        // A bar preset's lowColor and highColor are its scale colours.
        const auto barLabel=[](const QString& key) {
            if(key=="lowColor") {return tr("Scale low color");}
            if(key=="highColor") {return tr("Scale high color");}
            return fieldLabel(key);
        };
        for(const auto& key:keys) {if(auto* widget=control(key,props[key])) {fields->addRow(barLabel(key),widget);}}
        addFadeRows(fields);
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
    if(declared && reason.isEmpty()) {
        for(const auto& p:schema) {
            if(p.kind==ContentProperty::Kind::Reading) {bindings->addRow(p.label,declaredControl(p,advanced));}
        }
    }
    const bool applet=entry.typeId.startsWith("applet:");
    if(entry.typeId.startsWith(QStringLiteral("tx.")) && m_registry.hasWidgetFactory(entry.typeId)) {
        // A TX section row is a copy the TX applet makes and wires; the
        // factory never reads the entry's context.
        advanced->setEnabled(false);advanced->setToolTip(tr("These controls follow the TX panel; they have no source of their own."));
    } else if(entry.typeId.startsWith(QStringLiteral("rx.")) && m_registry.hasWidgetFactory(entry.typeId)) {
        // An RX section item is a copy the RX applet makes and wires; the
        // factory never reads the entry's context.
        advanced->setEnabled(false);advanced->setToolTip(tr("These controls follow the RX panel; they have no source of their own."));
    } else if((composite && composite->typeId()=="meter.clock") || entry.typeId=="CLOCK") {
        advanced->setEnabled(false);advanced->setToolTip(tr("This clock uses local time independently of radio and MMIO sources. Stored context is retained."));
    } else if((applet && entry.typeId!="applet:s_meter") || (widgetType && !declared)) {
        // A TX or RX section row is a copy its applet makes and wires, so it
        // follows the applet's radio and slice as the applet itself does.
        advanced->setEnabled(false);advanced->setToolTip(tr("This applet keeps its current radio and slice. Saved source choices are retained."));
    } else if(!m_item && !applet && !declared) {advanced->setEnabled(false);advanced->setToolTip(tr("Unsupported content retains its stored bindings."));}
    else if(declared && !reason.isEmpty()) {advanced->setEnabled(false);advanced->setToolTip(reason);}
    auto* source=new QComboBox(advanced); source->setObjectName("contentSlice");
    const int inherited=m_defaults.contains("sliceId")?m_defaults["sliceId"].toInt():m_defaults["rxSource"].toInt(1)-1;
    source->addItem(m_defaults.value("rxSourceMode").toString()==QStringLiteral("followSelectedRx")
        ? tr("Container default — Follow selected RX") : tr("Container default — Slice %1").arg(QChar('A'+inherited)),-1);
    for(int i=0;i<4;++i) { source->addItem(tr("Slice %1").arg(QChar('A'+i)),i); }
    source->addItem(tr("Follow selected RX"),QStringLiteral("followSelectedRx"));
    const QVariant selected=entry.context.value("rxSourceMode").toString()==QStringLiteral("followSelectedRx")
        ? QVariant(QStringLiteral("followSelectedRx")) : QVariant(entry.context.contains("sliceId")?entry.context["sliceId"].toInt():entry.context.contains("rxSource")?entry.context["rxSource"].toInt()-1:-1);
    source->setCurrentIndex(source->findData(selected)); bindings->addRow(tr("Source"),source);
    connect(source,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,source]{
        m_entry.context.remove("rxSource"); m_entry.context.remove("sliceId"); m_entry.context.remove("rxSourceMode");
        if(source->currentData().toString()==QStringLiteral("followSelectedRx")) {m_entry.context["rxSourceMode"]=QStringLiteral("followSelectedRx");}
        else if(source->currentData().toInt()>=0) {m_entry.context["sliceId"]=source->currentData().toInt();} publish();
    });
    if(composite && composite->typeId()=="meter.historyGraph") {
        // Signal history trace 2 picks its own slice (Off leaves it off).
        auto* trace2=new QComboBox(advanced); trace2->setObjectName("historyTrace2Source");
        trace2->addItem(tr("Off"),QStringLiteral("off"));
        for(int i=0;i<4;++i) { trace2->addItem(tr("Slice %1").arg(QChar('A'+i)),i); }
        trace2->addItem(tr("Follow selected RX"),QStringLiteral("followSelectedRx"));
        const QJsonValue stored=composite->configuration().value("trace2Context");
        const QJsonObject current=stored.toObject();
        const QVariant chosen=!stored.isObject() ? QVariant(QStringLiteral("off"))
            : current.value("rxSourceMode").toString()==QStringLiteral("followSelectedRx") ? QVariant(QStringLiteral("followSelectedRx"))
            : QVariant(current.contains("sliceId")?current["sliceId"].toInt():current["rxSource"].toInt()-1);
        trace2->setCurrentIndex(qMax(0,trace2->findData(chosen))); bindings->addRow(tr("Trace 2 source"),trace2);
        connect(trace2,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,trace2]{
            const QVariant data=trace2->currentData();
            if(data.toString()==QStringLiteral("off")) { property("trace2Context",QJsonValue(QJsonValue::Null)); return; }
            auto* face=qobject_cast<CompositePresetItem*>(m_item.get());
            QJsonObject next;
            // A radio session the trace names is kept.
            const QJsonObject before=face?face->trace2Context():QJsonObject();
            if(before.contains("sessionId")) { next["sessionId"]=before["sessionId"]; }
            if(data.toString()==QStringLiteral("followSelectedRx")) { next["rxSourceMode"]=QStringLiteral("followSelectedRx"); }
            else { next["sliceId"]=data.toInt(); }
            property("trace2Context",next);
        });
    }
    for(const QString& key:{QStringLiteral("sessionId"),QStringLiteral("mmioGuid"),QStringLiteral("mmioVariable")}) {
        // Declared widget types read no MMIO source.
        if((applet || declared || qobject_cast<WaveformMeterItem*>(m_item.get())) && key!="sessionId") {continue;}
        auto* edit=new QLineEdit(entry.context[key].toString(),advanced); edit->setObjectName(key);
        edit->setPlaceholderText(key=="sessionId"?tr("Container default — %1").arg(m_defaults[key].toString(tr("current session"))):tr("Unassigned"));
        bindings->addRow(key=="sessionId"?tr("Radio session"):key=="mmioGuid"?tr("MMIO source UUID"):tr("MMIO variable"),edit);
        if(key=="mmioGuid") {edit->setToolTip(tr("A data source's UUID or its 4-character ID"));}
        connect(edit,&QLineEdit::editingFinished,this,[this,edit,key]{
            // A 4-character data source ID is stored as that source's UUID.
            if(key=="mmioGuid" && edit->text().trimmed().size()==4 && QUuid(edit->text().trimmed()).isNull()) {
                if(MmioEndpoint* ep=ExternalVariableEngine::instance().endpointByFourChar(edit->text().trimmed())) {
                    QSignalBlocker block(edit); edit->setText(ep->guid().toString(QUuid::WithoutBraces));
                }
            }
            if(edit->text().isEmpty()) {m_entry.context.remove(key);} else {m_entry.context[key]=edit->text();}
            if(m_item && (key=="mmioGuid" || key=="mmioVariable")) {m_item->setMmioBinding(QUuid(m_entry.context["mmioGuid"].toString()),m_entry.context["mmioVariable"].toString());}
            publish(); });
    }
    // Variables 2 and up, one row pair per variable the type reads.
    const bool readsDataSource=m_item && !applet && !declared && !qobject_cast<WaveformMeterItem*>(m_item.get());
    const int variableCount=readsDataSource?MeterItem::mmioVariableCount(entry.typeId):0;
    for(int index=1;index<variableCount && index<MeterItem::kMaxMmioVariables;++index) {
        const QJsonObject stored=entry.context.value("mmioVariables").toArray().at(index-1).toObject();
        for(const QString& field:{QStringLiteral("guid"),QStringLiteral("variable")}) {
            const bool guidField=field=="guid";
            auto* edit=new QLineEdit(stored.value(field).toString(),advanced);
            edit->setObjectName((guidField?QStringLiteral("mmioGuid"):QStringLiteral("mmioVariable"))+QString::number(index));
            edit->setPlaceholderText(tr("Unassigned"));
            bindings->addRow(guidField?tr("MMIO source UUID, variable %1").arg(index+1):tr("MMIO variable %1").arg(index+1),edit);
            if(guidField) {edit->setToolTip(tr("A data source's UUID or its 4-character ID"));}
            connect(edit,&QLineEdit::editingFinished,this,[this,edit,index,field,guidField]{
                // A 4-character data source ID is stored as that source's UUID.
                if(guidField && edit->text().trimmed().size()==4 && QUuid(edit->text().trimmed()).isNull()) {
                    if(MmioEndpoint* ep=ExternalVariableEngine::instance().endpointByFourChar(edit->text().trimmed())) {
                        QSignalBlocker block(edit); edit->setText(ep->guid().toString(QUuid::WithoutBraces));
                    }
                }
                QJsonArray variables=m_entry.context.value("mmioVariables").toArray();
                while(variables.size()<index) {variables.append(QJsonObject{{"guid",QString()},{"variable",QString()}});}
                QJsonObject variable=variables.at(index-1).toObject();
                variable[field]=edit->text();
                if(!variable.contains("guid")) {variable["guid"]=QString();}
                if(!variable.contains("variable")) {variable["variable"]=QString();}
                variables[index-1]=variable;
                // Trailing unassigned variables are not stored.
                while(!variables.isEmpty() && variables.last().toObject().value("guid").toString().isEmpty() && variables.last().toObject().value("variable").toString().isEmpty()) {variables.removeLast();}
                if(variables.isEmpty()) {m_entry.context.remove("mmioVariables");} else {m_entry.context["mmioVariables"]=variables;}
                if(m_item) {m_item->setMmioBinding(index,QUuid(variable.value("guid").toString()),variable.value("variable").toString());}
                publish(); });
        }
    }
    m_error=new QLabel(this);m_error->setWordWrap(true);m_error->setStyleSheet("color:#ffb0b0");root->addWidget(m_error);
    for(auto* group:findChildren<QGroupBox*>()) {if(auto* form=qobject_cast<QFormLayout*>(group->layout())) {form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setFormAlignment(Qt::AlignLeft|Qt::AlignTop);form->setLabelAlignment(Qt::AlignLeft);}}
    root->addStretch(); m_loading=false;
}
}
