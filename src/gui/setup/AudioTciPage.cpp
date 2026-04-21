#include "AudioTciPage.h"

namespace NereusSDR {

AudioTciPage::AudioTciPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TCI"), model, parent)
{
    auto* lbl = new QLabel(QStringLiteral(
        "TCI server \u2014 coming in Phase 3J "
        "(see 2026-04-19-vax-design.md \u00a711.1)"));
    lbl->setStyleSheet(QStringLiteral(
        "color:#8090a0;font-size:12px;padding:40px;"));
    lbl->setAlignment(Qt::AlignCenter);
    contentLayout()->addWidget(lbl, 0, Qt::AlignCenter);
    contentLayout()->addStretch(1);
}

} // namespace NereusSDR
