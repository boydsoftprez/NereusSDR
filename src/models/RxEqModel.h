// =================================================================
// src/models/RxEqModel.h  (NereusSDR)
// =================================================================
//
// Global receive audio equalizer state: the ten-band graphic curve (the
// WDSP RXAEQ stage) and the parametric curve the same stage takes through
// SetRXAEQProfile. One model drives every slice's RX channel, which is how
// Thetis's console EQ works.
//
// The DSP application lives in RadioModel (it owns the RxChannels); this
// class owns the values, persistence and change signals.
// =================================================================

#pragma once

#include <QObject>
#include <QString>

#include "core/NereusCoreExport.h"

#include <array>

class QTimer;

namespace NereusSDR {

class NEREUS_CORE_EXPORT RxEqModel : public QObject {
    Q_OBJECT

public:
    explicit RxEqModel(QObject* parent = nullptr);

    // Ranges shared with the TX EQ controls.
    static constexpr int kPreampDbMin = -12;
    static constexpr int kPreampDbMax = 15;
    static constexpr int kBandDbMin = -12;
    static constexpr int kBandDbMax = 15;
    static constexpr int kFreqHzMin = 10;
    static constexpr int kFreqHzMax = 22000;
    static constexpr int kNcMin = 32;
    static constexpr int kNcMax = 8192;
    static constexpr int kCtfmodeMax = 1;
    static constexpr int kWintypeMax = 1;
    static constexpr int kBandCount = 10;
    static constexpr int kDefaultFreqs[kBandCount] = {
        32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

    void load();
    void save();

    bool rxEqEnabled() const noexcept { return m_enabled; }
    int  rxEqPreamp() const noexcept { return m_preampDb; }
    int  rxEqBand(int index) const noexcept;
    int  rxEqFreq(int index) const noexcept;
    bool rxEqUseLegacy() const noexcept { return m_useLegacy; }
    const QString& rxEqParaEqData() const noexcept { return m_paraEqData; }
    int  rxEqNc() const noexcept { return m_nc; }
    bool rxEqMp() const noexcept { return m_mp; }
    int  rxEqCtfmode() const noexcept { return m_ctfmode; }
    int  rxEqWintype() const noexcept { return m_wintype; }

    // The whole ten-band curve, [0] = preamp, [1..10] = band gains.
    std::array<int, 11> graph10() const;

public slots:
    void setRxEqEnabled(bool on);
    void setRxEqPreamp(int dB);
    void setRxEqBand(int index, int dB);
    void setRxEqFreq(int index, int hz);
    void setRxEqUseLegacy(bool legacy);
    void setRxEqParaEqData(const QString& data);
    void setRxEqNc(int nc);
    void setRxEqMp(bool mp);
    void setRxEqCtfmode(int mode);
    void setRxEqWintype(int wintype);

    // TF/N bypass path: the value already lives in Audio/RxEqEnabled and
    // must not be written again, only reflected.
    void applyBypassEnabled(bool on);

signals:
    void rxEqEnabledChanged(bool on);
    void rxEqPreampChanged(int dB);
    void rxEqBandChanged(int index, int dB);
    void rxEqFreqChanged(int index, int hz);
    void rxEqUseLegacyChanged(bool legacy);
    void rxEqParaEqDataChanged(const QString& data);
    void rxEqNcChanged(int nc);
    void rxEqMpChanged(bool mp);
    void rxEqCtfmodeChanged(int mode);
    void rxEqWintypeChanged(int wintype);

private:
    bool m_enabled{false};
    int  m_preampDb{0};
    std::array<int, kBandCount> m_bandsDb{};
    std::array<int, kBandCount> m_freqsHz{};
    bool m_useLegacy{true};
    QString m_paraEqData;
    int  m_nc{2048};
    bool m_mp{false};
    int  m_ctfmode{0};
    int  m_wintype{0};
    QTimer* m_saveTimer{nullptr};
};

} // namespace NereusSDR
