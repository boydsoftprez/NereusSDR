// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/applets/RoundtableApplet.h  (NereusSDR)
// =================================================================
//
// The Roundtable applet in the right column (design 11.2 Panel; mockup
// docs/architecture/reporter-brainstorm/mockups/placement.html): one
// RoundtablePanel that follows the active slice. Tune A here tunes the
// active slice to the room read; tuning the slice anywhere goes back to
// following it. Archive opens the web sign-in link; Settings asks its
// owner (MainWindow opens the Spot Hub on its Roundtable tab).
//
// Pop out moves the panel into a RoundtableWindow of the applet's own
// (createFloatingWindow, through AppletPanelWidget::floatApplet); the
// applet then says so with "Put it back here". Closing the window docks
// the panel back.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
//   2026-10-24: Roundtable plan Task 15: m_sliceMeter. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-24: tuneToRoom, for a panadapter tag (Roundtable plan Task 14).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "AppletWidget.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "models/RoundtableModel.h"

#include <QPointer>
#include <QUrl>

#include <functional>

class QVBoxLayout;

namespace NereusSDR {

class AppletPanelWidget;
class RoundtablePanel;
class SliceModel;

class RoundtableApplet : public AppletWidget {
    Q_OBJECT

public:
    /// The Roundtable of `model` (RadioModel::roundtableModel()).
    explicit RoundtableApplet(RadioModel* model, QWidget* parent = nullptr);
    /// For tests: a Roundtable of its own; `model` may be null.
    RoundtableApplet(RadioModel* model, RoundtableModel* roundtable, QWidget* parent);
    ~RoundtableApplet() override;

    QString appletId() const override { return QStringLiteral("roundtable"); }
    QString appletTitle() const override { return QStringLiteral("Roundtable"); }
    bool canFloat() const override { return true; }
    void syncFromModel() override;
    QWidget* createFloatingWindow() override;
    void dockFloatingWindow() override;

    RoundtablePanel* panel() const { return m_panel; }
    /// The pop-out window once made (it lives as long as the applet).
    RoundtableWindow* floatingWindow() const { return m_window; }
    bool isPoppedOut() const;
    RoundtableModel* roundtable() const { return m_roundtable; }

    /// Tunes the active slice to room `roomId` from the band list, as Tune
    /// A here does: a panadapter tag or a fold's row (design 11.2).
    void tuneToRoom(const QString& roomId);

    /// What opens the Archive's sign-in link (a seam for tests).
    void setUrlOpener(std::function<void(const QUrl&)> opener) { m_urlOpener = std::move(opener); }

signals:
    /// The panel's Settings: open Roundtable's settings.
    void settingsRequested();

private:
    void build();
    void bindActiveSlice();
    void tuneHere(qint64 hz, const QString& family);
    void openArchive();
    void popOut();
    void putBack();
    AppletPanelWidget* host() const;

    QPointer<RoundtableModel> m_roundtable;
    RoundtablePanel* m_panel = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QWidget* m_poppedNote = nullptr;
    QPointer<RoundtableWindow> m_window;
    QPointer<SliceModel> m_slice;
    QMetaObject::Connection m_sliceFrequency;
    QMetaObject::Connection m_sliceMeter;
    std::function<void(const QUrl&)> m_urlOpener;
    bool m_webLinkPending = false;
    bool m_tuning = false;
};

} // namespace NereusSDR
