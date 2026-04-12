#pragma once

#include "SliceModel.h"
#include "PanadapterModel.h"
#include "MeterModel.h"
#include "TransmitModel.h"
#include "core/RadioDiscovery.h"
#include "core/RadioConnection.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QThread>

namespace NereusSDR {

class ReceiverManager;
class AudioEngine;
class WdspEngine;

// RadioModel is the central data model for a connected radio.
// It owns the RadioConnection (on a worker thread), ReceiverManager,
// and all sub-models. It routes signals between components.
//
// Thread architecture:
//   Main thread: RadioModel, ReceiverManager, all sub-models, GUI
//   Connection thread: RadioConnection (sockets, protocol I/O)
//   Audio thread: AudioEngine + WdspEngine (future)
class RadioModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name        READ name        NOTIFY infoChanged)
    Q_PROPERTY(QString model       READ model       NOTIFY infoChanged)
    Q_PROPERTY(QString version     READ version     NOTIFY infoChanged)
    Q_PROPERTY(bool    connected   READ isConnected NOTIFY connectionStateChanged)

public:
    explicit RadioModel(QObject* parent = nullptr);
    ~RadioModel() override;

    // Sub-components
    RadioConnection*  connection()       { return m_connection; }
    const RadioConnection* connection() const { return m_connection; }
    RadioDiscovery*   discovery()        { return m_discovery; }
    ReceiverManager*  receiverManager()  { return m_receiverManager; }
    AudioEngine*      audioEngine()      { return m_audioEngine; }
    WdspEngine*       wdspEngine()       { return m_wdspEngine; }

    // Sub-models
    MeterModel&       meterModel()       { return m_meterModel; }
    TransmitModel&    transmitModel()    { return m_transmitModel; }

    // Slice management (client-side — radio has no slice concept)
    QList<SliceModel*> slices() const { return m_slices; }
    SliceModel* sliceAt(int index) const;
    SliceModel* activeSlice() const { return m_activeSlice; }
    int addSlice();
    void removeSlice(int index);
    void setActiveSlice(int index);

    // Panadapter management (client-side)
    QList<PanadapterModel*> panadapters() const { return m_panadapters; }
    int addPanadapter();
    void removePanadapter(int index);

    // View hooks: non-owning pointers to the primary spectrum widget and
    // FFT engine so setup pages (Phase 3G-8+) can call renderer/FFT
    // setters without depending on MainWindow. Wired by MainWindow after
    // constructing each view. Not owned, not lifetime-tracked — MainWindow
    // outlives both.
    class SpectrumWidget* spectrumWidget() const { return m_spectrumWidget; }
    void setSpectrumWidget(class SpectrumWidget* w) { m_spectrumWidget = w; }
    class FFTEngine* fftEngine() const { return m_fftEngine; }
    void setFftEngine(class FFTEngine* e) { m_fftEngine = e; }

    // Radio info
    QString name() const { return m_name; }
    QString model() const { return m_model; }
    QString version() const { return m_version; }
    bool isConnected() const;

    // Connection
    void connectToRadio(const RadioInfo& info);
    void disconnectFromRadio();

signals:
    void infoChanged();
    void connectionStateChanged();
    void sliceAdded(int index);
    void sliceRemoved(int index);
    void activeSliceChanged(int index);
    void panadapterAdded(int index);
    void panadapterRemoved(int index);

    // Raw interleaved I/Q for spectrum display (tapped before WDSP processing)
    void rawIqData(const QVector<float>& interleavedIQ);

private slots:
    void onConnectionStateChanged(NereusSDR::ConnectionState state);

private:
    void wireConnectionSignals();
    void wireSliceSignals();
    void teardownConnection();
    void loadSliceState(SliceModel* slice);
    void saveSliceState(SliceModel* slice);
    void scheduleSettingsSave();

    // Sub-components (owned, main thread)
    RadioDiscovery*  m_discovery{nullptr};
    ReceiverManager* m_receiverManager{nullptr};
    AudioEngine*     m_audioEngine{nullptr};
    WdspEngine*      m_wdspEngine{nullptr};

    // Connection (owned, lives on m_connThread)
    RadioConnection* m_connection{nullptr};
    QThread*         m_connThread{nullptr};

    // Sub-models
    MeterModel    m_meterModel;
    TransmitModel m_transmitModel;

    // Slices and panadapters (client-managed)
    QList<SliceModel*> m_slices;
    QList<PanadapterModel*> m_panadapters;
    SliceModel* m_activeSlice{nullptr};

    // View hooks (non-owning, set by MainWindow). Phase 3G-8.
    class SpectrumWidget* m_spectrumWidget{nullptr};
    class FFTEngine*      m_fftEngine{nullptr};

    // Radio info
    QString m_name;
    QString m_model;
    QString m_version;

    // Reconnect state
    RadioInfo m_lastRadioInfo;
    bool m_intentionalDisconnect{false};

    // I/Q accumulation buffer for WDSP (accumulates 238-sample P2 packets
    // until we have in_size samples for one fexchange2 call)
    // Thetis formula: in_size = 64 * rate / 48000 → 1024 at 768 kHz
    // WDSP output: out_size = in_size * out_rate / in_rate → 64 at 768k→48k
    static constexpr int kWdspBufSize = 1024;
    static constexpr int kWdspOutSize = 64;   // 1024 * 48000 / 768000
    QVector<float> m_iqAccumI;
    QVector<float> m_iqAccumQ;

    // Settings save coalescing
    bool m_settingsSaveScheduled{false};
};

} // namespace NereusSDR
