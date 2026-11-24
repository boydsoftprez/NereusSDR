#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QStringList>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <QVariant>

class QCheckBox;
class QShowEvent;

#include "gui/DisplayFftReadout.h"

#include <functional>
#include <optional>

namespace NereusSDR {

class RadioModel;
class SpectrumWidget;
class PanadapterModel;

struct SetupDisplayClarityStatus {
    bool requestedEnabled;
    bool active;
    bool paused;
    bool sourceAvailable;
};

// Display pill plan Task 21 (D26, D27): what a Setup page needs from the
// window to treat its display controls as the defaults for every pan.
// `panWidgets` is every pan's widget; `bandGridModel` is the active pan's
// band store (PanModels). The window gives both through
// SetupDialog::setPanWidgetsProvider and setBandGridModelProvider. A page on
// its own (a test, or a page with no window) has neither: it then takes the
// model's one widget as its only pan and has no band store.
struct SetupDisplayProviders {
    std::function<QList<SpectrumWidget*>()> panWidgets;
    std::function<PanadapterModel*()>       bandGridModel;
    std::function<PanadapterModel*(int)> bandGridModelForPan;
    // Available pan addresses, including those not open in this window.
    std::function<QList<int>()> panIndices;
    std::function<std::optional<SetupDisplayClarityStatus>(int)> clarityStatusForPan;
};

// Display pill plan Task 20 (D22, D26; R-DSP-33): how a Setup page changes
// the station's FFT settings. The window gives one handler per control
// (SetupDialog::setFftBaselineHandler and its neighbours; MainWindow points
// them at applyFftBaseline, applyDisplayFps, applyDisplayFftWindow,
// applyDisplayHzPerBinTarget and applyDisplayDecimation), so Setup and each
// pan's Display pill change them through one path: every stream's engine,
// one key write, every open pill brought up to date. `readoutFacts` is what
// the active pan's FFT runs at now. An empty handler means the page is on
// its own (a test, or a page with no window) and acts on the model's engine
// pool itself.
struct SetupFftHandlers {
    std::function<void(int)>    applyBaseline;
    std::function<void(int)>    applyFps;
    std::function<void(int)>    applyWindowType;
    std::function<void(double)> applyHzPerBinTarget;
    std::function<void(int)>    applyDecimation;
    std::function<DisplayFftReadoutFacts()> readoutFacts;
    // FFT sizing mode (design D38): the "Sizing" control, pointed at
    // MainWindow::applyDisplayFftSizingMode. The second value is the Hz/bin
    // target to start from when the station remembers none.
    std::function<void(ControlRanges::DisplayFftSizingMode, double)> applySizingMode;
};

// Base class for all setup dialog pages.
// Provides a consistent dark-themed layout with helper methods for
// building labeled rows of controls.
//
// Two constructor forms:
//   SetupPage(title, model, parent)  — for pages that may later need model access
//   SetupPage(title, parent)         — for pages that only use raw Qt widgets
class SetupPage : public QWidget {
    Q_OBJECT
public:
    // Construct with a RadioModel pointer (may be nullptr).
    explicit SetupPage(const QString& title, RadioModel* model, QWidget* parent = nullptr);
    // Convenience overload for pages that don't need RadioModel access.
    explicit SetupPage(const QString& title, QWidget* parent = nullptr);
    ~SetupPage() override;

    QString pageTitle() const { return m_title; }

    // The title every Setup page draws above its content. A page that is
    // not a SetupPage (RF-Kit's tabbed page) draws its title with this too.
    static QLabel* makePageTitle(const QString& title, QWidget* parent = nullptr);
    virtual void syncFromModel();

    // R-R3-21: a receive page can hold a transmit section. SetupDialog pushes
    // the negotiated transmit permission to every realized page; a page with
    // such a section overrides this and gates just that section, leaving the
    // rest of the page live. The default does nothing.
    virtual void setTransmitPermitted(bool permitted, const QString& reason);

    // Fix wave 2 (M8): in a remote window VOX also needs this computer's
    // microphone line to the Core. SetupDialog pushes it to every realized
    // page; a page holding a VOX control overrides this and shows it
    // disabled with `reason` while there is none. The default does nothing.
    virtual void setVoxPermitted(bool permitted, const QString& reason);

    // R-R3-49 (parity Task 1): a page can hold transmit settings that key
    // nothing (DSP > Options TX combos). SetupDialog pushes the transmit
    // settings gate to every realized page; a page with such settings
    // overrides this and gates just them. The default does nothing.
    virtual void setTransmitSettingsPermitted(bool permitted, const QString& reason);

    // R-R3-49 (parity Task 3): the same gate for settings a later
    // transmitSettingsVersion brought (3: the radio microphone settings and
    // the TX profiles). SetupDialog pushes each version MainWindow gives it;
    // a page overrides this and gates the settings of the versions it
    // holds. Never called in a local window. The default does nothing.
    virtual void setTransmitSettingsPermittedAt(int version, bool permitted,
                                                const QString& reason);

    // R-R3-21 / R-R3-10: whether the Core's settings can be changed from
    // this window right now. False in a remote window while it is not
    // connected to its Core (or has not received the Core's settings yet).
    // SetupDialog pushes it to every realized page; a Mixed page overrides
    // this and gates just the controls that write the Core's settings,
    // leaving this computer's own controls live. The default does nothing.
    // Always true in a local window.
    virtual void setStationSettingsAvailable(bool available, const QString& reason);

    // Display pill plan Task 20: the window's FFT handlers (SetupFftHandlers
    // above). SetupDialog pushes them to every realized page; a page with
    // FFT controls reads them through fftHandlers(), and overrides this to
    // show what they now report. The default only keeps them.
    virtual void setFftHandlers(const SetupFftHandlers& handlers);

    // One of the station's FFT settings (size, window, Hz/bin target, frames
    // per second, decimation) was changed from outside this dialog: a pan's
    // Display pill, or, in a remote window, the Core. SetupDialog tells
    // every realized page; a page showing them overrides this, shows them
    // again under signal blockers and writes nothing. The default does
    // nothing.
    virtual void stationFftSettingsChanged();

    // Display pill plan Task 21: the window's pans and band store
    // (SetupDisplayProviders above). SetupDialog pushes them to every
    // realized page. A page that follows display defaults starts following
    // the pans they name and shows the defaults again.
    void setDisplayProviders(const SetupDisplayProviders& providers);

    // The approved Display scope: -1 edits All pans; a non-negative index
    // edits that pan's picture, including one that is not open yet.
    Q_INVOKABLE void setDisplayPanIndex(int panIndex);
    Q_INVOKABLE void enableDisplayPanScope();
    Q_INVOKABLE void enableDisplayBandPicker();
    Q_INVOKABLE void addDisplayBackgroundSection();
    Q_INVOKABLE void addDisplayGradientEditor(bool transmit);
    Q_INVOKABLE void convertManualWaterfallModesForSmoothReset();
    // A regrouped page operates only on the picture rows it displays.
    void setDisplayPageKeyBases(const QStringList& keys);
    int displayPanIndex() const { return m_displayPanIndex; }
    std::optional<SetupDisplayClarityStatus> displayClarityStatus(int panIndex) const
    {
        return m_displayProviders.clarityStatusForPan ? m_displayProviders.clarityStatusForPan(panIndex) : std::nullopt;
    }
    Q_INVOKABLE void resetDisplayPageOverrides();
    Q_INVOKABLE void copyDisplayPageToAll();
    QStringList displayPageKeyBases() const;
    int displayPageOwnCount(int panIndex) const;

signals:
    void displayPanIndexChanged(int panIndex);

public:

    // ── Section builder ───────────────────────────────────────────────────────
    // Add a titled group box section to the page content area.
    QGroupBox* addSection(const QString& title);

    // R-SPK-21: add a widget or a layout to the page content area, before
    // the trailing stretch as addSection() does, so the page's content
    // starts at the top. Pages use these instead of appending straight to
    // contentLayout(), which would put the item after the stretch.
    void addContent(QWidget* widget);
    void addContent(QLayout* layout);

    // ── Convenience row builders ──────────────────────────────────────────────
    // Each creates a labeled row (150px label + control) and adds it to the
    // current section. Returns the created control widget.

    // Toggle button (checkable QPushButton styled as an LED toggle).
    QPushButton* addLabeledToggle(const QString& label);

    // Combo box populated with the given items.
    QComboBox* addLabeledCombo(const QString& label, const QStringList& items);

    // Horizontal slider.
    QSlider* addLabeledSlider(const QString& label, int minimum, int maximum, int value);

    // Integer spin box.
    QSpinBox* addLabeledSpinner(const QString& label, int minimum, int maximum, int value);

    // Push button with custom text.
    QPushButton* addLabeledButton(const QString& label, const QString& buttonText);

    // Read-only label showing a value string.
    QLabel* addLabeledLabel(const QString& label, const QString& value);

    // Single-line text edit with optional placeholder.
    QLineEdit* addLabeledEdit(const QString& label, const QString& placeholder = {});

protected:
    // R-R3-21: disables each control with `reason` as its tooltip and
    // accessible description while transmit is not permitted, and puts back
    // the enabled state, tooltip and description each had once it is. Safe
    // to call repeatedly with the same state.
    static void gateTransmitControls(const QList<QWidget*>& controls, bool permitted,
                                     const QString& reason);

    // R-R3-21 / R-R3-10: the same, for the controls that write the Core's
    // settings on a Mixed page, while those settings are unavailable. It
    // keeps its own saved state, so a control must follow only one of the
    // two helpers; a page with a control held for both combines the two
    // conditions and calls one of them.
    static void gateStationControls(const QList<QWidget*>& controls, bool available,
                                    const QString& reason);

    // The same again, for controls locked while the radio is on the air
    // (a page's own on-the-air rule). It keeps its own saved state, so a
    // control follows only this helper; a control also held for transmit
    // combines the two conditions and calls gateTransmitControls.
    static void gateOnAirControls(const QList<QWidget*>& controls, bool offAir,
                                  const QString& reason);

    QVBoxLayout* contentLayout() { return m_contentLayout; }
    RadioModel*  model()         { return m_model; }
    const SetupFftHandlers& fftHandlers() const { return m_fftHandlers; }

    // ── Display defaults (Display pill plan Task 21) ─────────────────────────
    // Setup's display controls are the defaults every pan shows until it is
    // given a value of its own (core/settings/PanDisplayKeys.h). A pan's own
    // value is set from that pan's Display pill, never from here.

    // Every pan's widget (the provider's list without nulls; the model's one
    // widget for a page on its own).
    QList<SpectrumWidget*> panWidgets() const;
    // The active pan's band store, or null when there is no pan.
    PanadapterModel* bandGridModel() const;

    // Writes the Setup master for `keyBase`, clearing all its pan overrides,
    // then calls `apply` on every open pan. The default is written first on
    // purpose: a pan's own save writes a key only when its value differs
    // from what it inherits (PanDisplayKeys::writeSparse), so a pan moved to
    // the new default stores nothing and keeps following Setup. `value` is
    // formatted the way the key is stored ("True"/"False", a number, a
    // colour's "#AARRGGBB" name).
    void applyDisplayDefault(const QString& keyBase, const QVariant& value,
                             const std::function<void(SpectrumWidget*)>& apply);

    // The Setup default for `keyBase` as stored: the bare key when it holds
    // something, else the ship default. Empty when there is no pan to ask
    // for the ship default (and for a setting whose ship default is empty).
    QString displayDefaultText(const QString& keyBase) const;

    // Names the code that shows `keyBase`'s default on this page. It runs
    // when that default may have changed (another Setup page, or a pan that
    // announced the key) and from showDisplayDefaults(); it sets the control
    // under a QSignalBlocker and writes nothing.
    void followDisplayDefault(const QString& keyBase, std::function<void()> show);
    // Shows every followed default again.
    void showDisplayDefaults();

    // One control bound to one per-pan default: a change goes through
    // applyDisplayDefault, and the control follows the default. `spin` is the
    // number cell paired with a slider (makeSliderRow), kept in step when
    // the slider is set quietly.
    void bindDefaultToggle(QCheckBox* box, const QString& keyBase,
                           std::function<void(SpectrumWidget*, bool)> apply);
    void bindDefaultCombo(QComboBox* combo, const QString& keyBase,
                          std::function<void(SpectrumWidget*, int)> apply);
    void bindDefaultSlider(QSlider* slider, QSpinBox* spin, const QString& keyBase,
                           std::function<void(SpectrumWidget*, int)> apply);
    void bindDefaultSpin(QSpinBox* spin, const QString& keyBase,
                         std::function<void(SpectrumWidget*, int)> apply);

    // Called when the pans this page follows changed, one of them said
    // whether Clarity is setting its levels, or a followed default was shown
    // again. A page whose rows depend on another row's state (Waterfall
    // Defaults' levels) redoes that here. The default does nothing.
    virtual void displayPansChanged();

    // The one pan a page built against a widget, not the radio model, knows
    // until Setup hands it the pan list (the 3D View page).
    void setFallbackPan(SpectrumWidget* pan);

    void showEvent(QShowEvent* event) override;

    // The same save-and-restore gate as the three above, for a page's own
    // "something else is setting this" rule (Waterfall Defaults' levels).
    static void gateOwnedControls(const QList<QWidget*>& controls, bool live,
                                  const QString& reason);

    // ── Low-level helpers (for subclasses that build complex layouts) ─────────
    // Add a labeled row where the control widget is pre-created.
    QHBoxLayout* addLabeledCombo(QLayout* parent, const QString& label, QComboBox* combo);
    QHBoxLayout* addLabeledSlider(QLayout* parent, const QString& label, QSlider* slider,
                                   QLabel* valueLabel = nullptr);
    QHBoxLayout* addLabeledToggle(QLayout* parent, const QString& label, QPushButton* toggle);
    QHBoxLayout* addLabeledSpinner(QLayout* parent, const QString& label, QSpinBox* spinner);
    QHBoxLayout* addLabeledEdit(QLayout* parent, const QString& label, QLineEdit* edit);
    QHBoxLayout* addLabeledLabel(QLayout* parent, const QString& label, QLabel* value);

    // A widget in the section most recently opened with addSection().
    void addToSection(QWidget* widget);

private:
    void init(const QString& title);
    QHBoxLayout* makeLabeledRow(QLayout* parent, const QString& labelText, QWidget* control);
    // Display pill plan Task 21: connects this page to each pan it has not
    // met yet, so a followed default is shown again when a pan announces it.
    void followPanWidgets();
    void showDisplayDefault(const QString& keyBase);

    QString       m_title;
    RadioModel*   m_model          = nullptr;
    SetupFftHandlers m_fftHandlers;
    SetupDisplayProviders m_displayProviders;
    int m_displayPanIndex{-1};
    SpectrumWidget* selectedDisplayPan() const;
    QHash<QString, std::function<void()>> m_displayFollowers;
    QStringList m_displayScopeKeys;
    bool m_hasDisplayScopeKeys{false};
    QWidget* m_displayScopeStrip{nullptr};
    QBoxLayout* m_displayScopeChipLayout{nullptr};
    QHash<int, QPushButton*> m_displayScopeChips;
    QLabel* m_displayScopeNote{nullptr};
    QPushButton* m_displayScopeReset{nullptr};
    QPushButton* m_displayScopeCopy{nullptr};
    void refreshDisplayPanScope();
    QComboBox* m_displayBandPicker{nullptr};
    QMetaObject::Connection m_displayBandConnection;
    void refreshDisplayBandPicker();
    QSet<QObject*> m_followedPans;
    QPointer<QObject> m_fallbackPan;
    QVBoxLayout*  m_contentLayout  = nullptr;
    QVBoxLayout*  m_activeSectionLayout = nullptr;  // layout of the last added section
};

} // namespace NereusSDR
