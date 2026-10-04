#pragma once
// no-port-check: NereusSDR-original content catalog and lossless meter adapter.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerDocument.h"
#include <QObject>
#include <QMetaType>
#include <QPointer>
#include <QHash>
#include <QWidget>
namespace NereusSDR {
class MeterItem;
enum class ContentRenderMode { Live, Preview, Validation };
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
    ContentEntry captureMeterItem(const MeterItem& item, const ContentEntry& prior = {}) const;
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
    QWidget* m_parking = nullptr;
    quint64 m_generation = 0;
};
} // namespace NereusSDR
Q_DECLARE_METATYPE(NereusSDR::ContentEntry)
