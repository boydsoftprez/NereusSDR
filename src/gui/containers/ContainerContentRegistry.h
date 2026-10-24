#pragma once
// no-port-check: NereusSDR-original content catalog and lossless meter adapter.
// Modification history (NereusSDR):
//   2026-10-08 - Workspace preview only: ContentConfigNotifier, through
//                 which a per-instance widget's own controls change its
//                 entry's config. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: a widget type declares its
//                 editable properties (ContentProperty) for the Container
//                 Settings property editor. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: widget factories take a render
//                 mode, and createPreview passes the effective context and
//                 ContentRenderMode::Preview. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-20 - Integer and Color property kinds for Frequency (Custom).
//                 Brian Sutton, AI-assisted via xAI Grok.
#include "ContainerDocument.h"
#include <QObject>
#include <QMetaType>
#include <QPointer>
#include <QHash>
#include <QJsonValue>
#include <QVector>
#include <QWidget>
#include <functional>
namespace NereusSDR {
class MeterItem;
enum class ContentRenderMode { Live, Preview, Validation };
// Workspace preview: one choice of a declared property.
struct ContentPropertyChoice {
    QString label;
    QJsonValue value;           // Null: the key is removed from the config.
    bool enabled = true;
    QString disabledReason;
};
// Workspace preview: an editable property a per-instance widget type
// declares, the counterpart of a MeterItem's editableFields() for content
// that has no MeterItem. ContentPropertyEditor builds its controls from
// these and writes the value to the entry's config under `key`; the edit
// goes to the draft like every other property. A Reading is a choice of
// reading binding and sits with the other bindings.
struct ContentProperty {
    enum class Kind { Reading, Choice, Toggle, Text, Integer, Color };
    QString key, label;
    Kind kind = Kind::Choice;
    QVector<ContentPropertyChoice> choices;
    // The value shown when the config has no `key`.
    QJsonValue defaultValue;
    // When set, the value shown when the config has no `key`, from the
    // rest of the config (a default that depends on another key).
    std::function<QJsonValue(const QJsonObject& config)> effectiveDefault;
    QString placeholder;        // Text: shown while the text is empty.
    // Integer: the spin box range and the suffix after the number.
    int minimum = 0;
    int maximum = 1000000;
    QString suffix;
};
// Workspace preview: a per-instance widget whose own controls change its
// entry's config (the SIGNAL STRENGTH dial's right-click menu) gives itself
// a direct child of this class and emits configEdited with the entry's
// whole new config. ContainerContentHost commits it to the workspace
// document (ContainerArrangeController::setEntryConfig). The factory also
// sets the widget's kAppliedConfigProperty to that config: the widget
// already shows it, so the host keeps the widget instead of rebuilding it.
class ContentConfigNotifier : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    static constexpr const char* kAppliedConfigProperty = "containerAppliedConfig";
signals:
    void configEdited(const QJsonObject& config);
};
struct ContentDescriptor {
    QString typeId, title;
    bool singleton = false, available = false;
    QString unavailableReason;
};
class ContainerContentRegistry : public QObject {
    Q_OBJECT
public:
    explicit ContainerContentRegistry(QObject* parent = nullptr);
    ~ContainerContentRegistry() override;
    void attachSingleton(const QString& typeId, QWidget* liveWidget);
    QWidget* singletonView(const QString& typeId) const;
    QSize singletonCanvasMinimum(const QString& typeId) const;
    QSize singletonCanvasSizeHint(const QString& typeId) const;
    void setAvailable(const QString& typeId, bool available, const QString& reason = {});
    bool isAvailable(const QString& typeId) const;
    QString unavailableReason(const QString& typeId) const;
    void parkSingleton(QWidget* widget);
    void returnBorrowedView(QWidget* widget);
    void setSingletonPlacements(const QHash<QString, QString>& placements);
    bool claimSingleton(const QString& typeId, const QString& entryId);
    quint64 generation() const { return m_generation; }
    static QString appletTypeForVisibilityId(const QString& id);
    QVector<ContentDescriptor> descriptors() const;
    ContentEntry makeEntry(const QString& typeId) const;
    QString validateEntry(const ContentEntry& entry) const;
    MeterItem* createMeterItem(const ContentEntry& entry, QObject* parent,
                              ContentRenderMode mode = ContentRenderMode::Live) const;
    QWidget* createPreview(const ContentEntry& entry, QWidget* parent) const;
    // As above, for an entry shown under `context`, the effective source
    // context (ContainerContentHost::effectiveContext) the live host uses.
    QWidget* createPreview(const ContentEntry& entry, QWidget* parent, const QJsonObject& context) const;
    ContentEntry captureMeterItem(const MeterItem& item, const ContentEntry& prior = {}) const;

    // Per-instance QWidget content (workspace preview: the Longpath
    // instruments). Unlike attachSingleton, the host creates one widget per
    // entry through the factory. The factory receives the entry with its
    // context replaced by the effective source context (container default
    // merged with the entry, see ContainerContentHost::effectiveContext),
    // and the parent the widget goes into. A factory returns nullptr when
    // it cannot build the entry; the host then shows its explanation row.
    // In ContentRenderMode::Preview the widget is inert: it registers no
    // reading consumer, follows no slice and runs no timer, and shows the
    // cached readings for its context.
    using WidgetFactory = std::function<QWidget*(const ContentEntry& entry, QWidget* parent,
                                                 ContentRenderMode mode)>;
    // `properties`: the type's editable properties (workspace preview).
    void registerWidgetFactory(const QString& typeId, const ContentDescriptor& descriptor, WidgetFactory factory,
                               const QVector<ContentProperty>& properties = {});
    // The properties a widget type declared; empty for every other type.
    QVector<ContentProperty> declaredProperties(const QString& typeId) const;
    void unregisterWidgetFactory(const QString& typeId);
    bool hasWidgetFactory(const QString& typeId) const;
    QWidget* createWidget(const ContentEntry& entry, QWidget* parent,
                          ContentRenderMode mode = ContentRenderMode::Live) const;
signals:
    void runtimeChanged();
private:
    struct NativeSizes { QSize minimum, maximum, hint, editMinimum; };
    QHash<QWidget*,NativeSizes> m_nativeSizes;
    void restoreNativeConstraints(QWidget*);
    QHash<QString, QPointer<QWidget>> m_singletons;
    QHash<QWidget*, QPointer<QWidget>> m_originalParents;
    QHash<QString, QWidget*> m_attachedIdentities;
    QHash<QString, QMetaObject::Connection> m_destroyConnections;
    QHash<QString, QString> m_placements;
    QHash<QString, QString> m_unavailable;
    struct WidgetFactoryRecord { ContentDescriptor descriptor; WidgetFactory factory; QVector<ContentProperty> properties; };
    QHash<QString, WidgetFactoryRecord> m_widgetFactories;
    QStringList m_widgetFactoryOrder;
    QWidget* m_parking = nullptr;
    quint64 m_generation = 0;
};
} // namespace NereusSDR
Q_DECLARE_METATYPE(NereusSDR::ContentEntry)
