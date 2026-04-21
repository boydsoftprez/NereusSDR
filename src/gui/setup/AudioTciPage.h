#pragma once

#include "gui/SetupPage.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Audio > TCI
//
// Placeholder page for the TCI server configuration UI. Full implementation
// lands in Phase 3J (see 2026-04-19-vax-design.md §11.1).
// ---------------------------------------------------------------------------
class AudioTciPage : public SetupPage {
    Q_OBJECT
public:
    explicit AudioTciPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
