#pragma once
// no-port-check: NereusSDR-original draft content property adapter.
#include "ContainerContentRegistry.h"
#include "gui/NereusGuiExport.h"
#include <functional>
#include <memory>
class QLabel;
class QFormLayout;
namespace NereusSDR {
class MeterItem;
class NEREUS_GUI_EXPORT ContentPropertyEditor : public QWidget {
    Q_OBJECT
public:
    explicit ContentPropertyEditor(ContainerContentRegistry& registry, QWidget* parent=nullptr);
    ~ContentPropertyEditor() override;
    void setEntry(const ContentEntry& entry);
    void setCanvasPresentation(const QRectF& rect,const QSizeF& minimum) {m_resolvedRect=rect;m_editMinimum=minimum;}
    void setLayoutPolicy(ContentLayout policy) {m_policy=policy;}
    void setGeometryLocked(bool locked);
    void updateFreeCanvasRect(const QString& id,const QRectF& rect);
    void setContainerDefaults(const QJsonObject& defaults);
signals:
    void entryEdited(const ContentEntry& entry);
private:
    QWidget* control(const QString& key, const QJsonValue& value, int channel=-1);
    // Workspace preview: a control for a declared ContentProperty.
    QWidget* declaredControl(const ContentProperty& property, QWidget* parent);
    void captureRenderer();
    // Fade on RX / Fade on TX rows for a bar preset, composite or waveform
    // face: they set the item's fade (saved in the entry context); a
    // composite's legacy fadeRx / fadeTx configuration is turned off.
    void addFadeRows(QFormLayout* fields);
    void publish();
    QJsonValue effectiveProperty(const QString& key, int channel) const;
    bool property(const QString& key, const QJsonValue& value, int channel=-1);
    ContainerContentRegistry& m_registry;
    ContentEntry m_entry;
    QJsonObject m_defaults;
    QRectF m_resolvedRect=QRectF(0,0,320,80);
    QSizeF m_editMinimum=QSizeF(24,24);
    ContentLayout m_policy=ContentLayout::LegacyCanvas;
    std::unique_ptr<MeterItem> m_item;
    ContentEntry m_original, m_hydrated;
    QLabel* m_error=nullptr;
    bool m_loading=false, m_geometryLocked=false;
};
}
