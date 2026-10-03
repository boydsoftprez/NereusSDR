#pragma once
// =================================================================
// src/gui/SliceChooser.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The bottom RX area's all-slice
// chooser (slice control and shared listening plan Task 13; JJ's approved
// bottom area, the reviewed mockup nereus-slice-chooser-review.html).
//
// Every slice on the Core, each with its letter and colour, frequency, mode
// and filter, who controls it (this window, the Core's own desktop, another
// device by the name the Core numbers, or nobody), whether that device is
// away, this window's listening state and whether it is transmitting.
// Choosing a row only shows it; the actions act: Listen in, Select RX, Take
// control, Release, Stop listening and New slice. A request shows as
// waiting until the Core answers, then the answer. With no slice the window
// offers New slice in honest words.
//
// A view only: MainWindow builds the rows (rowsForRemoteWindow,
// rowsForHostingDesktop) and carries each request to the Core. Local audio
// in a row (ruling U5, "Your volume") comes with Task 14b.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: created for NereusSDR by J.J. Boyd (KG4VCF), slice control
//               and shared listening plan Task 13, with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: Row::takeRefusal. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: desktop listening lane: Row::controllerName names the
//               hosting desktop for CoreDesktop. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

#include "gui/widgets/VfoWidget.h"

class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;

namespace NereusSDR {

class RadioModel;
class RemoteDevicesState;
class SliceAccessMirror;
class StationServer;

class SliceChooser : public QWidget {
    Q_OBJECT

public:
    enum class Controller {
        ThisWindow,   ///< this window controls it
        CoreDesktop,  ///< the station device: the desktop hosting the Core, or the Core itself
        OtherDevice,  ///< another device, named in controllerName
        Nobody,       ///< nobody controls it
    };

    struct Row {
        int sliceId = -1;
        QColor color;
        double frequencyHz = 0.0;
        QString mode;
        QString filter;
        Controller controller = Controller::Nobody;
        /// The Core's name for the controlling device (numbered by the Core
        /// when two share a name), for OtherDevice. For CoreDesktop, the
        /// name of the desktop that hosts the Core (its connectedDevices
        /// entry with hostsCore), or empty on a Core no desktop hosts.
        QString controllerName;
        /// The controlling device is away (or the Core keeps the slice for
        /// a device that is not here).
        bool controllerAway = false;
        bool listeningHere = false;
        /// The bottom RX area follows this slice.
        bool activeHere = false;
        /// Devices joined to it, the controller included.
        int listenerCount = 0;
        bool transmitting = false;
        /// Core-slice take-over (JJ, 2026-09-30): why Take control is not
        /// offered (the Core's own words), or empty when it is. A Core
        /// below sliceAccessVersion 3 refuses a take of its own slice.
        QString takeRefusal;

        QChar letter() const { return QChar(QLatin1Char('A').unicode() + sliceId); }
    };

    explicit SliceChooser(QWidget* parent = nullptr);

    void setInventory(const QList<Row>& rows);
    QList<Row> inventory() const { return m_rows; }
    /// No room for another slice: New slice says existing slices can be
    /// shared.
    void setNoRoomForNewSlice(bool full);
    /// The row shown in detail (-1: none).
    void selectSlice(int sliceId);
    int selectedSliceId() const { return m_selected; }
    /// A request is on its way: its actions wait until the answer.
    void setPending(const QString& waitingWords);
    /// The Core's answer (or a refusal before sending): shown, and the
    /// actions are live again.
    void showResult(const QString& words);
    bool isPending() const { return m_pending; }
    QString message() const;
    /// A request sent to the Core: `verb` names the answer that finishes
    /// it; `success` is shown when it is accepted.
    void beginRequest(const QByteArray& verb, const QString& waitingWords,
                      const QString& success);
    /// The Core's answer for `verb`: shown, and true, only when it is the
    /// request in flight.
    bool finishRequest(const QByteArray& verb, bool accepted, const QString& reason);
    QByteArray requestInFlight() const { return m_requestVerb; }
    /// The link to the Core dropped: a waiting request will not be
    /// answered, so it is cleared and says so.
    void linkLost();
    /// Opened again: a wait with no request in flight is cleared.
    void reopened();

    // ---- Builders (MainWindow; tests) ----
    /// A remote window: its own and listened slices from `model`, every
    /// other device's from `devices`' markers, control and listening from
    /// `access` (null for a Core that does not share slices).
    static QList<Row> rowsForRemoteWindow(const RadioModel& model, const SliceAccessMirror* access,
                                          const RemoteDevicesState& devices);
    /// The hosting desktop: every slice of `model`, controller and away
    /// from its ownership and `server`'s device words.
    static QList<Row> rowsForHostingDesktop(RadioModel& model, StationServer& server);
    /// The banner's words for this window's active slice: "You control",
    /// "Listening", or "Choose a slice" with none.
    static QString bannerState(const QList<Row>& rows);
    /// Task 14a: the slice flag's access for `row`. Controlled when this
    /// window controls it, Listening (naming the controller) when this
    /// window only listens to it, Unshared otherwise.
    static VfoWidget::SliceAccess flagAccessFor(const Row& row);

signals:
    void listenRequested(int sliceId);
    void takeControlRequested(int sliceId);
    void releaseRequested(int sliceId);
    void stopListeningRequested(int sliceId);
    void newSliceRequested();
    void selectRequested(int sliceId);
    void closeRequested();
    /// Every answer shown (the Core's, a refusal before sending, or "The
    /// Core did not answer"), so a flag that sent the request can show it.
    void resultShown(const QString& words);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void rebuild();
    void rebuildDetail();
    QString ownerWords(const Row& row) const;
    QString stateWords(const Row& row) const;
    QString descriptionFor(const Row& row) const;
    QPushButton* actionButton(const QString& words, bool primary, bool enabled);

    QList<Row> m_rows;
    int m_selected = -1;
    bool m_full = false;
    bool m_pending = false;
    QByteArray m_requestVerb;
    QString m_requestSuccess;
    QVBoxLayout* m_list = nullptr;
    QWidget* m_detail = nullptr;
    QLabel* m_selectedLabel = nullptr;
    QLabel* m_description = nullptr;
    QHBoxLayout* m_actions = nullptr;
    QPushButton* m_newSlice = nullptr;
    QLabel* m_capacity = nullptr;
    QLabel* m_message = nullptr;
};

} // namespace NereusSDR
