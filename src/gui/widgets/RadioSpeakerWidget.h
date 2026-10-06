#pragma once

// =================================================================
// src/gui/widgets/RadioSpeakerWidget.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original widget.
//
// The header's RADIO group (R-SPK-17, D1): the radio's own speaker, which
// the Core owns and every window and the phone share. It sits beside the
// PC group (MasterOutputWidget) in the title bar, in the same shape: icon
// button, word label, 100 px slider and inset readout, with an amber
// slider so the two read apart at a glance (header-layouts.html layout A).
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan,
//                Task 6 (R-SPK-06, R-SPK-07, R-SPK-16, R-SPK-17, D1, D5,
//                D10). J.J. Boyd (KG4VCF), with AI-assisted implementation
//                via Anthropic Claude Code.
// =================================================================

#include <QString>
#include <QWidget>

class QEvent;
class QLabel;
class QPushButton;
class QSlider;

namespace NereusSDR {

class RadioModel;

// RadioSpeakerWidget: [radio icon 20] [RADIO] [slider 100] [readout 22].
//
// - radioSpeakerBtn (checkable, 20 x 20): a click toggles
//   RadioModel::radioSpeakerMuted (D5). Icons radio-on, radio-muted and,
//   while there is no radio speaker to set, radio-none; the button's
//   AppIcon::kIconProperty names the icon shown.
// - radioSlider (0 to 100) writes RadioModel::radioSpeakerVolume.
// - radioValueLabel shows the level, or "--" while unavailable.
//
// Model changes from any source (Setup, a remote window, the phone, a
// reload on connect) update the widget without writing back
// (m_updatingFromModel plus QSignalBlocker).
//
// Availability 0 (no radio, or a Core that can't set it): the button and
// slider are disabled, never hidden (D10), and the tooltip gives the
// reason. Otherwise the tooltip is RadioModel::radioSpeakerToolTip(): in a
// remote window it says RADIO is the speaker at the Core (R-SPK-16), and
// on a Hermes Lite 2 it carries the audio add-on board note (R-SPK-07).
// A null model reads as no radio connected.
class RadioSpeakerWidget : public QWidget {
    Q_OBJECT
public:
    explicit RadioSpeakerWidget(RadioModel* model, QWidget* parent = nullptr);

    // Binds the widget to another model (or none). The title bar is built
    // before MainWindow hands it the model (TitleBar::setRadioModel).
    void setRadioModel(RadioModel* model);
    RadioModel* radioModel() const { return m_model; }

protected:
    // Recomputes the tooltip just before it shows, so a reason that
    // changed without an availability change (a remote window attaching
    // to an older Core) is current.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void syncFromModel();
    void refreshToolTip();
    void applyIcon(bool available, bool muted);

    // Cleared when the model is destroyed first.
    RadioModel*  m_model{nullptr};
    QPushButton* m_button{nullptr};
    QLabel*      m_label{nullptr};
    QSlider*     m_slider{nullptr};
    QLabel*      m_valueLabel{nullptr};
    bool         m_updatingFromModel{false};
};

} // namespace NereusSDR
