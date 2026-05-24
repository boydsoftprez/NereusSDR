// =================================================================
// src/gui/applets/Rf2ksApplet.cpp  (NereusSDR-native)
// =================================================================
//   2026-05-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   Layout patterns from src/gui/applets/AmpApplet.{h,cpp} (which is
//   an AetherSDR port). The RF-Kit-specific content is original.
// =================================================================

#include "Rf2ksApplet.h"
#include "models/RadioModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace NereusSDR {

Rf2ksApplet::Rf2ksApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header row: device label + nickname/version on the left;
    // status dot + OPERATE/STANDBY button on the right.
    auto* headerWrap = new QWidget(this);
    auto* header = new QHBoxLayout(headerWrap);
    header->setContentsMargins(8, 6, 8, 6);

    auto* leftCol = new QVBoxLayout();
    m_deviceLabel   = new QLabel(QStringLiteral("RF-Kit RF2K-S"), headerWrap);
    m_nicknameLabel = new QLabel(QString(), headerWrap);
    m_deviceLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    m_nicknameLabel->setStyleSheet(QStringLiteral("color:#7d8893; font-size:10px;"));
    leftCol->addWidget(m_deviceLabel);
    leftCol->addWidget(m_nicknameLabel);
    header->addLayout(leftCol);
    header->addStretch();

    m_statusDot = new QLabel(headerWrap);
    m_statusDot->setFixedSize(10, 10);
    setConnectedState(false);
    header->addWidget(m_statusDot);

    m_operateBtn = new QPushButton(QStringLiteral("STANDBY"), headerWrap);
    m_operateMode = QStringLiteral("STANDBY");
    connect(m_operateBtn, &QPushButton::clicked, this, [this]() {
        emit operateToggled(m_operateMode != QStringLiteral("OPERATE"));
    });
    header->addWidget(m_operateBtn);

    root->addWidget(headerWrap);
}

void Rf2ksApplet::setNicknameAndVersion(const QString& nickname, const QString& version)
{
    m_nicknameLabel->setText(QStringLiteral("%1  %2").arg(nickname, version));
}

void Rf2ksApplet::setOperateMode(const QString& mode)
{
    m_operateMode = mode;
    m_operateBtn->setText(mode);
}

void Rf2ksApplet::setConnectedState(bool connected)
{
    m_connected = connected;
    const QString color = connected
        ? QStringLiteral("#34c759")
        : QStringLiteral("#e64949");
    m_statusDot->setStyleSheet(
        QStringLiteral("background:%1; border-radius:5px;").arg(color));
}

QString Rf2ksApplet::deviceLabelTextForTesting()   const { return m_deviceLabel->text(); }
QString Rf2ksApplet::nicknameLabelTextForTesting() const { return m_nicknameLabel->text(); }
QString Rf2ksApplet::operateButtonTextForTesting() const { return m_operateBtn->text(); }
void    Rf2ksApplet::clickOperateButtonForTesting() { m_operateBtn->click(); }

} // namespace NereusSDR
