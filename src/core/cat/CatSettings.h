// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// Independently implemented from the approved Nereus settings schema; no upstream serialization logic.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "CatConfiguration.h"
#include <QList>
namespace NereusSDR {
class AppSettings; class RadioModel;
class CatSettings {
public:
    explicit CatSettings(AppSettings& settings);
    static QList<CatEndpointConfig> load(AppSettings&, const RadioModel&);
    static void save(AppSettings&, const CatEndpointConfig&);
    static bool validate(const CatEndpointConfig&, QString* reason = nullptr);
    CatGlobalConfig global() const;
    bool setGlobal(const CatGlobalConfig&);
private:
    AppSettings& m_settings;
};
} // namespace NereusSDR
