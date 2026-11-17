// =================================================================
// src/gui/applets/AppletKeys.cpp  (Longpath)
// =================================================================
// Siehe AppletKeys.h — eine Kennung, nicht zwei.
// =================================================================

#include "gui/applets/AppletKeys.h"

#include "gui/applets/AppletWidget.h"

namespace Longpath {
namespace AppletKeys {

QString panelIdFor(const AppletMap& map, const AppletWidget* applet)
{
    if (!applet) { return {}; }
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.value() == applet) { return it.key(); }
    }
    return applet->appletId();
}

AppletWidget* appletFor(const AppletMap& map, const QString& key)
{
    if (key.isEmpty()) { return nullptr; }
    // Erst die Panelkennung — sie ist der Normalfall und eine
    // Hash-Abfrage. Der Durchlauf unten ist der Rueckfall fuer alte
    // Aufnahmen und laeuft ueber neunzehn Eintraege.
    if (AppletWidget* a = map.value(key, nullptr)) { return a; }
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.value() && it.value()->appletId() == key) { return it.value(); }
    }
    return nullptr;
}

QString canonical(const AppletMap& map, const QString& key)
{
    AppletWidget* a = appletFor(map, key);
    return a ? panelIdFor(map, a) : key;
}

QStringList kombiInhaltOhneEigeneFenster(const QStringList& gewuenscht,
                                         const QStringList& schwebend)
{
    QStringList bleibt;
    for (const QString& id : gewuenscht) {
        if (id.isEmpty()) { continue; }
        if (schwebend.contains(id)) { continue; }   // eigenes Fenster gewinnt
        if (bleibt.contains(id)) { continue; }
        bleibt << id;
    }
    return bleibt;
}

} // namespace AppletKeys
} // namespace Longpath
