#pragma once

// =================================================================
// src/gui/applets/AppletVisibilityController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. No Thetis equivalent — Thetis exposes
// container-level show/hide via setup checkboxes, not per-applet
// toggles. AetherSDR has no equivalent.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Backs the Containers >
//                 Applets menu and the right-side panel ☰ menu.
// =================================================================

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

namespace NereusSDR {

class AppletVisibilityController : public QObject {
    Q_OBJECT
public:
    explicit AppletVisibilityController(QObject* parent = nullptr);

    // Register an applet's id, display name, and default visibility.
    // If an AppSettings key for this id already exists, the persisted
    // value wins over defaultVisible. Idempotent on the same id; later
    // calls overwrite the display name but preserve current state.
    void registerApplet(const QString& id,
                        const QString& displayName,
                        bool defaultVisible);

    bool isVisible(const QString& id) const;          // user preference
    bool isAvailable(const QString& id) const;        // capability gate
    bool isEffectivelyVisible(const QString& id) const; // visible && available
    QStringList registeredIds() const;       // insertion order preserved
    QString displayName(const QString& id) const;

public slots:
    // User preference — persisted to AppSettings.
    void setVisible(const QString& id, bool visible);

    // Capability gate — NOT persisted. Lets the controller hide an
    // applet (and grey its menu entry) when an external feature flag
    // says the applet shouldn't be reachable. Example: when the 4O3A
    // master toggle is off, the Amplifier and Tuner applets become
    // unavailable. The user's persisted visibility preference is
    // preserved across availability changes, so re-enabling 4O3A pops
    // the applet back if the user wanted it visible.
    void setAvailable(const QString& id, bool available);

signals:
    // User preference changed (e.g., user clicked the menu).
    void visibilityChanged(const QString& id, bool visible);

    // Capability gate changed (e.g., 4O3A master toggle flipped).
    void availabilityChanged(const QString& id, bool available);

    // Convenience signal: fires when EITHER axis changes if it caused
    // the effective visibility to flip. Consumers that just want to
    // show/hide the wrapper should connect here.
    void effectiveVisibilityChanged(const QString& id, bool effective);

private:
    static QString settingsKey(const QString& id);  // "AppletRxVisible" etc.

    struct Entry {
        QString displayName;
        bool visible{true};       // user pref (persisted)
        bool available{true};     // capability gate (runtime)
    };
    QHash<QString, Entry> m_entries;   // id -> entry
    QStringList m_order;               // registration order
};

} // namespace NereusSDR
