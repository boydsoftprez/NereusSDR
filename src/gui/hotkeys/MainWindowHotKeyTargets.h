// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 3.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// What hot keys do in the real window: the same entry points the on-screen
// controls use, so local, hosting and remote windows behave as those do.
#pragma once

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/HotKeyTargets.h"

#include <QElapsedTimer>

class QObject;
class QWidget;

namespace NereusSDR {

class MainWindow;
class SpectrumWidget;
class VfoWidget;

class NEREUS_GUI_EXPORT MainWindowHotKeyTargets final : public HotKeyTargets {
public:
    explicit MainWindowHotKeyTargets(MainWindow* window);

    SliceModel* windowSlice() const override;
    SliceModel* sliceByLetter(QChar letter) const override;
    QChar letterOf(const SliceModel* slice) const override;
    QString sliceRefusal(const SliceModel* slice) const override;
    bool listened(const SliceModel* slice) const override;
    bool tuneTo(SliceModel* slice, double hz) override;
    void runSliceAction(const QString& id, SliceModel* slice, int steps) override;
    void runWindowAction(const QString& id) override;
    QString unavailableReason(const QString& id) const override;
    bool transmitting() const override;
    bool tuneOrTwoToneOn() const override;
    bool keyHold(bool down) override;
    void runTransmitAction(const QString& id) override;
    void runCwMessage(const QString& id) override;
    void setCwKeyDown(const QString& id, bool down) override;
    void toast(const QString& text) override;
    qint64 nowMs() const override;

    // The pan zoom acts on while `activeWindow` has focus: that floating pan,
    // else the window's active pan.
    SpectrumWidget* zoomTarget(const QWidget* activeWindow) const;

    // Band stack is reached by slot name, so these keys stay disabled with a
    // reason until `window` has the slots.
    static QString bandStackReason(const QObject* window, const QString& id);
    static bool runBandStack(QObject* window, const QString& id);

private:
    VfoWidget* flagOf(const SliceModel* slice) const;
    void clickContainerButton(const QString& id, SliceModel* slice);
    void changeVolume(SliceModel* slice, int delta);
    void toggleMute(SliceModel* slice);
    void setListenVolume(int sliceId, int level, bool muted);
    void beginFrequencyEdit(const SliceModel* slice);
    void walkFilter(SliceModel* slice, int direction);
    void walkMode(SliceModel* slice, int direction);
    void walkBand(SliceModel* slice, int direction);
    void selectNextSlice();

    MainWindow* m_window;
    QElapsedTimer m_clock;
};

} // namespace NereusSDR
