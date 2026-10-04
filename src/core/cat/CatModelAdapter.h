// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "CatTypes.h"
#include "core/WdspTypes.h"
#include <QPointer>
namespace NereusSDR {
class RadioModel; class SliceModel; class TransmitModel;
struct CatWriteToken {
    CatBinding binding; CatVfo vfo{CatVfo::Primary}; QByteArray property;
    quint64 controlRevision{0}; bool admitted{false};
};
class CatModelAdapter {
public:
    explicit CatModelAdapter(RadioModel& model);
    CatBinding snapshotBinding(const CatBinding&) const;
    bool mayRead(const CatBinding&, CatVfo) const;
    bool mayChange(const CatBinding&, CatVfo, const QByteArray& property) const;
    SliceModel* resolveSlice(const CatBinding&, CatVfo) const;
    TransmitModel& transmitModel() const;
    RadioModel& radioModel() const;
    CatWriteToken prepareWrite(const CatBinding&, CatVfo, const QByteArray& property) const;
    bool revalidateWrite(const CatWriteToken&) const;
    bool mayChangeGlobalDsp(QString* reason = nullptr) const;
    bool readRxMeter(const CatBinding&, CatVfo, RxMeterType, double& value) const;
private:
    QPointer<RadioModel> m_model;
};
} // namespace NereusSDR
