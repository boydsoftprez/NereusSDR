// =================================================================
// src/gui/AuxiliaryWindowLeveler.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: AuxiliaryWindowLeveler.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "AuxiliaryWindowLeveler.h"

#include "gui/MacFloatingWindowBehavior.h"

#include <QEvent>
#include <QGuiApplication>
#include <QTimer>
#include <QWidget>

namespace Longpath {

AuxiliaryWindowLeveler::AuxiliaryWindowLeveler(QObject* parent)
    : QObject(parent)
{
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState) { onApplicationStateChanged(); });
}

bool AuxiliaryWindowLeveler::isAuxiliaryWindow(const QWidget* w)
{
    if (!w || !w->isWindow() || !w->parentWidget()) { return false; }
    const Qt::WindowFlags flags = w->windowFlags();
    const Qt::WindowType type = static_cast<Qt::WindowType>(
        static_cast<int>(flags & Qt::WindowType_Mask));
    if (type != Qt::Window && type != Qt::Dialog) { return false; }
    // "Bleibt oben" liegt bei Qt ohnehin ueber den Paletten.
    if (flags.testFlag(Qt::WindowStaysOnTopHint)) { return false; }
    return true;
}

bool AuxiliaryWindowLeveler::istPalette(const QWidget* w)
{
    if (!w || !w->isWindow() || !w->parentWidget()) { return false; }
    const Qt::WindowFlags flags = w->windowFlags();
    const Qt::WindowType type = static_cast<Qt::WindowType>(
        static_cast<int>(flags & Qt::WindowType_Mask));
    if (type != Qt::Tool) { return false; }
    if (flags.testFlag(Qt::WindowStaysOnTopHint)) { return false; }
    return true;
}

bool AuxiliaryWindowLeveler::wirdGestuft(const QWidget* w)
{
    return isAuxiliaryWindow(w) || istPalette(w);
}

bool AuxiliaryWindowLeveler::eventFilter(QObject* watched, QEvent* event)
{
    // Die Reihenfolge, wie der Betreiber sie sieht: was er anklickt,
    // was aktiv wird, was neu aufgeht, liegt vorne. Der Klick zaehlt mit,
    // weil eine angeklickte Palette (NSPanel) nicht immer aktiv wird,
    // aber nach vorne kommt.
    if (event->type() == QEvent::WindowActivate
        || event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::Show) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            QWidget* top = w->window();
            if (top && (event->type() == QEvent::MouseButtonPress || w == top)) {
                touch(top);
            }
        }
    }

    // QEvent::Show kommt VOR dem eigentlichen Anzeigen (show_sys), das
    // native Fenster existiert aber schon -- die Ebene greift also fuer
    // das erste orderFront. Beim erneuten Zeigen kommt es wieder.
    if (event->type() == QEvent::Show) {
        // Seit 2026-11-15 auch die Paletten (Qt::Tool): sie verstecken
        // sich nicht mehr beim Verlassen der App, sie werden
        // heruntergestuft. Siehe den Kopf und
        // MacFloatingWindowBehavior.mm.
        if (auto* w = qobject_cast<QWidget*>(watched); w && wirdGestuft(w)) {
            track(w);
            applyLevel(w);
        }
    }
    return QObject::eventFilter(watched, event);
}

void AuxiliaryWindowLeveler::track(QWidget* w)
{
    if (m_tracked.contains(w)) { return; }
    m_tracked.insert(w);
    connect(w, &QObject::destroyed, this, [this, w]() { m_tracked.remove(w); });
}

void AuxiliaryWindowLeveler::applyLevel(QWidget* w) const
{
    const bool active = qApp->applicationState() == Qt::ApplicationActive;
    setPaletteWindowLevel(w, active);
}

void AuxiliaryWindowLeveler::onApplicationStateChanged()
{
    for (QWidget* w : m_tracked) {
        if (w->isVisible()) { applyLevel(w); }
    }
    // Zurueck in der App: macOS zeigt die Paletten wieder und legt sie
    // dabei alle VOR das Fenster, das der Betreiber zuletzt vorne hatte
    // (Test 2026-09-26: nach Finder -> Longpath lagen Rotor/Log und
    // Panadapter wieder vor dem Logbuch). Einen Takt spaeter, wenn AppKit
    // fertig sortiert hat, die zuletzt gesehene Reihenfolge herstellen.
    if (qApp->applicationState() == Qt::ApplicationActive) {
        QTimer::singleShot(50, this, [this]() { restoreRecentOrder(); });
    }
}

void AuxiliaryWindowLeveler::touch(QWidget* top)
{
    // Nur richtige Fenster: Menues, Tooltips und Aufklapplisten gehen und
    // kommen, sie sind keine Lage, die sich jemand merkt.
    const Qt::WindowType type = static_cast<Qt::WindowType>(
        static_cast<int>(top->windowFlags() & Qt::WindowType_Mask));
    if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::SplashScreen
        || type == Qt::Drawer || type == Qt::Desktop) {
        return;
    }
    for (int i = m_recent.size() - 1; i >= 0; --i) {
        if (!m_recent.at(i) || m_recent.at(i) == top) { m_recent.removeAt(i); }
    }
    m_recent.prepend(top);
    while (m_recent.size() > 24) { m_recent.removeLast(); }
}

void AuxiliaryWindowLeveler::restoreRecentOrder()
{
    // Von hinten nach vorne heben: das zuletzt benutzte Fenster wird als
    // letztes gehoben und liegt vorne. Die Ebenen bleiben, wie sie sind --
    // das Hauptfenster (normale Ebene) bleibt unter den Paletten.
    for (int i = m_recent.size() - 1; i >= 0; --i) {
        QWidget* w = m_recent.at(i);
        if (w && w->isVisible() && !w->isMinimized()) { w->raise(); }
    }
}

QList<QWidget*> AuxiliaryWindowLeveler::recentOrderForTest() const
{
    QList<QWidget*> out;
    for (const QPointer<QWidget>& w : m_recent) { if (w) { out << w; } }
    return out;
}

} // namespace Longpath
