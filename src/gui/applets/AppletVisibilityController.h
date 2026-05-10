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

    bool isVisible(const QString& id) const;
    QStringList registeredIds() const;       // insertion order preserved
    QString displayName(const QString& id) const;

public slots:
    void setVisible(const QString& id, bool visible);

signals:
    // Emitted only when the value actually changes.
    void visibilityChanged(const QString& id, bool visible);

private:
    static QString settingsKey(const QString& id);  // "AppletRxVisible" etc.

    struct Entry {
        QString displayName;
        bool visible{true};
    };
    QHash<QString, Entry> m_entries;   // id -> entry
    QStringList m_order;               // registration order
};

} // namespace NereusSDR
