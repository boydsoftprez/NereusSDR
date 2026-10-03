// =================================================================
// src/core/TxDisplayFeed.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. The one owner of the transmit
// analyzer's view for every viewer (a local pan, a remote window's pan).
// It cites Thetis's DisplayThread and getLowHighForRXn for the behaviour it
// follows; it translates no Thetis code.
//
// Remote-window parity Task 28 (A11, R-R3-49): while the radio is keyed,
// Thetis shows the transmit analyzer, never the receiver, on the
// transmitting receiver's display when display duplex is off:
//   Thetis console.cs:24281-24338 [v2.10.3.15] (DisplayThread,
//   `if (bLocalMox && !_display_duplex)` then
//   `GetPixels(cmaster.inid(1, 0), 0, ...)` for the trace and
//   `GetPixels(cmaster.inid(1, 0), 1, ...)` for the waterfall),
// and that display follows XIT while transmitting:
//   Thetis console.cs:22069-22150 [v2.10.3.15] (getLowHighForRXn,
//   "xit, only when txing", `if (chkXIT.Checked && !_display_duplex)`).
//
// The feed runs the analyzer on every key, viewers or not (the Core's
// DaemonApp did that before it; the analyzer's siphon must always point at a
// live display). A viewer registers the view its pan asks for; the governing
// viewer sets the analyzer's window and pixel count: the local viewer when
// there is one, otherwise the lowest id. With no viewer at all the feed
// leaves the window and pixel count as they are (a local window that has
// not moved onto the feed yet sets them itself).
//
// Modification history (NereusSDR):
//   2026-09-26 : Created for remote-window parity Task 28 (R-R3-49, A11,
//                 R-IOS-13) by J.J. Boyd (KG4VCF). AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-26 : Tasks 27-29 fix wave (R-R3-49): a remote viewer's view
//                 changes while keyed reach the analyzer at most once per
//                 kRemoteViewCoalesceMs, the last one always; the view goes
//                 to the analyzer with one SetAnalyzer. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/TxAnalyzer.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

#include <map>
#include <memory>
#include <set>

namespace NereusSDR {

class RadioModel;
class SliceModel;

class TxDisplayFeed : public QObject {
    Q_OBJECT

public:
    TxDisplayFeed(RadioModel* model, TxAnalyzer* analyzer, QObject* parent = nullptr);
    ~TxDisplayFeed() override;

    /// Thetis's transmit display window before any viewer asks for one:
    /// display.cs:1284-1295 [v2.10.3.15] (tx_display_low -4000,
    /// tx_display_high 4000).
    static constexpr int kDefaultLowHz = -4000;
    static constexpr int kDefaultHighHz = 4000;
    /// A remote viewer's view changes while keyed (a pan drag across the
    /// network) reach the analyzer at most once per this many ms: each one
    /// is a SetAnalyzer, which holds the section the TX DSP thread's
    /// Spectrum0 takes. The first goes at once, the last always follows.
    /// NereusSDR-original; a local viewer's changes are not held.
    static constexpr int kRemoteViewCoalesceMs = 50;

    /// A pan that shows the transmit display. `local` is a window running
    /// its own DSP (or hosting a Core); it governs whenever it is present.
    /// Returns the viewer's id (1 and up, never reused).
    int addViewer(double centreHz, double spanHz, int pixels, bool local,
                  bool mini = false);
    void updateViewer(int id, double centreHz, double spanHz, int pixels);
    void removeViewer(int id);

    /// The view the analyzer runs now (while keyed, the governing viewer's;
    /// the carrier follows the transmit slice's frequency and XIT).
    TxDisplayView currentView() const { return m_view; }
    bool isGoverning(int id) const;
    int governingViewer() const;
    bool isKeyed() const noexcept { return m_keyed; }
    int viewerCount() const noexcept { return static_cast<int>(m_viewers.size()); }

    TxAnalyzer* analyzer() const { return m_analyzer.data(); }
    /// The analyzer's FFT size and output rate, for a viewer's context.
    int fftSize() const;
    int outputFps() const;
    void setLocalMiniDemand(bool wanted);
    void stopMini();
    TxDisplayView miniView() const;
    int miniFftSize() const;
    int miniOutputFps() const;
    bool miniReady() const;
    bool miniAttachmentSettledForTest() const { return m_miniAttachSettled; }
    bool isMiniViewer(int id) const;

signals:
    void viewChanged(const NereusSDR::TxDisplayView& view);
    /// The governing viewer changed (a viewer came or went) without the
    /// view necessarily moving; a viewer's "shared" state follows it.
    void governorChanged(int viewerId);
    /// The analyzer's trace (pixout 0) and waterfall (pixout 1), while keyed.
    void traceReady(const QVector<float>& dbm);
    void waterfallReady(const QVector<float>& dbm);
    void miniViewChanged(const NereusSDR::TxDisplayView& view);
    void miniTraceReady(const QVector<float>& dbm);
    void miniWaterfallReady(const QVector<float>& dbm);
    void keyedChanged(bool keyed);

private:
    struct Viewer {
        double centreHz{0.0};
        double spanHz{0.0};
        int pixels{0};
        bool local{false};
        TxDisplayView lastGood;
    };

    void onMoxStateChanged(bool keyed);
    void watchTransmitSlice();
    /// Recomputes the carrier and the governing viewer's view; while keyed,
    /// applies it to the analyzer and emits viewChanged when it moved.
    void recompute();
    void ensureMini();
    void updateMiniView();
    void applyMiniRxSettings();
    double carrierHz() const;

    QPointer<RadioModel> m_model;
    QPointer<TxAnalyzer> m_analyzer;
    std::map<int, Viewer> m_viewers;
    std::set<int> m_miniViewerIds;
    bool m_localMiniDemand{false};
    bool m_miniAttached{false};
    bool m_miniAttachPending{false};
    bool m_miniAttachSettled{false};
    int m_miniChannelId{-1};
    quint64 m_miniEpoch{0};
    std::unique_ptr<TxAnalyzer> m_miniAnalyzer;
    int m_nextViewerId{1};
    int m_governor{0};
    bool m_keyed{false};
    TxDisplayView m_view;
    QPointer<SliceModel> m_watchedSlice;
    /// When a remote viewer's change last reached the analyzer, and the
    /// timer that brings a held one in.
    QElapsedTimer m_remoteApplied;
    QTimer m_remoteViewTimer;
    QList<QMetaObject::Connection> m_sliceConnections;
};

} // namespace NereusSDR
