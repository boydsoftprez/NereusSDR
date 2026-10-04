// --- From setup.cs ---
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Ported from Thetis Project Files/Source/Console/setup.cs and setup.Designer.cs
// Upstream setup.Designer.cs has no top-of-file GPL header; project-level LICENSE applies.
// Modification history (NereusSDR):
// 2026-10-04 - CAT preference validation and serialization by J.J. Boyd
//              (KG4VCF), AI-assisted via OpenAI Codex.
// Serialization is Nereus-original; source constants retain their contract.

#include "CatSettings.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include <QHostAddress>
namespace NereusSDR {
namespace {
QString boolString(bool value) { return value ? QStringLiteral("True") : QStringLiteral("False"); }
bool serialFormat(int baud, const QString& parity, int bits, const QString& stops)
{
    return baud > 0 && QStringList{"None", "Odd", "Even", "Mark", "Space"}.contains(parity)
        && bits >= 5 && bits <= 8 && QStringList{"1", "1.5", "2"}.contains(stops);
}
}
CatSettings::CatSettings(AppSettings& settings) : m_settings(settings) {}
QList<CatEndpointConfig> CatSettings::load(AppSettings& settings, const RadioModel& model)
{
    QList<CatEndpointConfig> result;
    for (int channel = 1; channel <= 4; ++channel) {
        CatEndpointConfig config; config.channel = channel;
        if (channel == 1) {
            config.tcpPort = CatDefaults::kFirstTcpPort;
            if (model.sliceById(0)) { config.binding.primarySliceId = 0; }
            if (model.sliceById(1)) { config.binding.secondarySliceId = 1; }
        }
        int secondary = config.binding.secondarySliceId.value_or(-1);
        const QString prefix = QStringLiteral("Cat/Channels/%1/").arg(channel);
    config.binding.primarySliceId = settings.value(prefix + "PrimarySliceId", config.binding.primarySliceId).toInt();
    secondary = settings.value(prefix + "SecondarySliceId", secondary).toInt();
    config.tcpEnabled = settings.value(prefix + "TcpEnabled", boolString(config.tcpEnabled)).toString() == "True";
    config.serialEnabled = settings.value(prefix + "SerialEnabled", boolString(config.serialEnabled)).toString() == "True";
    config.ptyEnabled = settings.value(prefix + "PtyEnabled", boolString(config.ptyEnabled)).toString() == "True";
    config.rigctldEnabled = settings.value(prefix + "RigctldEnabled", boolString(config.rigctldEnabled)).toString() == "True";
    config.tcpBindAddress = settings.value(prefix + "TcpBindAddress", config.tcpBindAddress).toString();
    config.rigctldBindAddress = settings.value(prefix + "RigctldBindAddress", config.rigctldBindAddress).toString();
    config.tcpPort = settings.value(prefix + "TcpPort", config.tcpPort).toInt();
    config.rigctldPort = settings.value(prefix + "RigctldPort", config.rigctldPort).toInt();
    config.serialDevice = settings.value(prefix + "SerialDevice", config.serialDevice).toString();
    config.serialBaud = settings.value(prefix + "SerialBaud", config.serialBaud).toInt();
    config.serialParity = settings.value(prefix + "SerialParity", config.serialParity).toString();
    config.serialDataBits = settings.value(prefix + "SerialDataBits", config.serialDataBits).toInt();
    config.serialStopBits = settings.value(prefix + "SerialStopBits", config.serialStopBits).toString();
    config.ptyDialect = settings.value(prefix + "PtyDialect", config.ptyDialect).toString();
        if (secondary >= 0) { config.binding.secondarySliceId = secondary; }
        else { config.binding.secondarySliceId.reset(); }
        result.append(config);
    }
    return result;
}
void CatSettings::save(AppSettings& settings, const CatEndpointConfig& config)
{
    if (config.channel < 1 || config.channel > 4) { return; }
    const QString prefix = QStringLiteral("Cat/Channels/%1/").arg(config.channel);
    settings.setValue(prefix + "PrimarySliceId", config.binding.primarySliceId);
    settings.setValue(prefix + "SecondarySliceId", config.binding.secondarySliceId.value_or(-1));
    settings.setValue(prefix + "TcpEnabled", boolString(config.tcpEnabled));
    settings.setValue(prefix + "SerialEnabled", boolString(config.serialEnabled));
    settings.setValue(prefix + "PtyEnabled", boolString(config.ptyEnabled));
    settings.setValue(prefix + "RigctldEnabled", boolString(config.rigctldEnabled));
    settings.setValue(prefix + "TcpBindAddress", config.tcpBindAddress);
    settings.setValue(prefix + "RigctldBindAddress", config.rigctldBindAddress);
    settings.setValue(prefix + "TcpPort", config.tcpPort);
    settings.setValue(prefix + "RigctldPort", config.rigctldPort);
    settings.setValue(prefix + "SerialDevice", config.serialDevice);
    settings.setValue(prefix + "SerialBaud", config.serialBaud);
    settings.setValue(prefix + "SerialParity", config.serialParity);
    settings.setValue(prefix + "SerialDataBits", config.serialDataBits);
    settings.setValue(prefix + "SerialStopBits", config.serialStopBits);
    settings.setValue(prefix + "PtyDialect", config.ptyDialect);
}
bool CatSettings::validate(const CatEndpointConfig& config, QString* reason)
{
    const auto fail = [reason](const QString& text) { if (reason) { *reason = text; } return false; };
    if (config.channel < 1 || config.channel > 4 || config.binding.primarySliceId < -1
        || (config.binding.secondarySliceId && *config.binding.secondarySliceId < 0)) {
        return fail(QStringLiteral("Invalid CAT channel or slice identity."));
    }
    if (config.tcpPort < 0 || config.tcpPort > 65535 || (config.tcpEnabled && config.tcpPort == 0)
        || config.rigctldPort < 0 || config.rigctldPort > 65535 || (config.rigctldEnabled && config.rigctldPort == 0)
        || QHostAddress(config.tcpBindAddress).isNull() || QHostAddress(config.rigctldBindAddress).isNull()) {
        return fail(QStringLiteral("Invalid CAT listener address or port."));
    }
    if ((config.serialEnabled && config.serialDevice.trimmed().isEmpty())
        || !serialFormat(config.serialBaud, config.serialParity, config.serialDataBits, config.serialStopBits)) {
        return fail(QStringLiteral("Invalid CAT serial device or format."));
    }
    if (config.ptyDialect != "Thetis" && config.ptyDialect != "Rigctld") { return fail(QStringLiteral("Invalid CAT PTY dialect.")); }
    if (reason) { reason->clear(); }
    return true;
}
CatGlobalConfig CatSettings::global() const
{
    CatGlobalConfig config;
    const QString prefix = QStringLiteral("Cat/");
    config.sendWelcome = m_settings.value(prefix + "SendWelcome", boolString(config.sendWelcome)).toString() == "True";
    config.rigIdentity = m_settings.value(prefix + "RigIdentity", config.rigIdentity).toString();
    config.allowKenwoodAi = m_settings.value(prefix + "AllowKenwoodAi", boolString(config.allowKenwoodAi)).toString() == "True";
    config.aiEnabled = m_settings.value(prefix + "AiEnabled", boolString(config.aiEnabled)).toString() == "True";
    config.aiSerial1 = m_settings.value(prefix + "AiSerial1", boolString(config.aiSerial1)).toString() == "True";
    config.aiSerial2 = m_settings.value(prefix + "AiSerial2", boolString(config.aiSerial2)).toString() == "True";
    config.aiSerial3 = m_settings.value(prefix + "AiSerial3", boolString(config.aiSerial3)).toString() == "True";
    config.aiSerial4 = m_settings.value(prefix + "AiSerial4", boolString(config.aiSerial4)).toString() == "True";
    config.aiTcp = m_settings.value(prefix + "AiTcp", boolString(config.aiTcp)).toString() == "True";
    config.digitalReportsSideband = m_settings.value(prefix + "DigitalReportsSideband", boolString(config.digitalReportsSideband)).toString() == "True";
    config.recenterVfo = m_settings.value(prefix + "RecenterVfo", boolString(config.recenterVfo)).toString() == "True";
    config.serialNumber = m_settings.value(prefix + "SerialNumber", config.serialNumber).toString();
    config.limitReportedPower = m_settings.value(prefix + "LimitReportedPower", boolString(config.limitReportedPower)).toString() == "True";
    config.rttyOffsetAEnabled = m_settings.value(prefix + "RttyOffsetAEnabled", boolString(config.rttyOffsetAEnabled)).toString() == "True";
    config.rttyOffsetBEnabled = m_settings.value(prefix + "RttyOffsetBEnabled", boolString(config.rttyOffsetBEnabled)).toString() == "True";
    config.rttyDiguHz = m_settings.value(prefix + "RttyDiguHz", config.rttyDiguHz).toInt();
    config.rttyDiglHz = m_settings.value(prefix + "RttyDiglHz", config.rttyDiglHz).toInt();
    config.pttEnabled = m_settings.value(prefix + "Ptt/Enabled", boolString(config.pttEnabled)).toString() == "True";
    config.pttDeviceSource = m_settings.value(prefix + "Ptt/DeviceSource", config.pttDeviceSource).toString();
    config.pttSerialDevice = m_settings.value(prefix + "Ptt/SerialDevice", config.pttSerialDevice).toString();
    config.pttUseCts = m_settings.value(prefix + "Ptt/UseCts", boolString(config.pttUseCts)).toString() == "True";
    config.pttUseDsr = m_settings.value(prefix + "Ptt/UseDsr", boolString(config.pttUseDsr)).toString() == "True";
    config.pttChannel = m_settings.value(prefix + "Ptt/Channel", config.pttChannel).toInt();
    config.pttSerialBaud = m_settings.value(prefix + "Ptt/SerialBaud", config.pttSerialBaud).toInt();
    config.pttSerialParity = m_settings.value(prefix + "Ptt/SerialParity", config.pttSerialParity).toString();
    config.pttSerialDataBits = m_settings.value(prefix + "Ptt/SerialDataBits", config.pttSerialDataBits).toInt();
    config.pttSerialStopBits = m_settings.value(prefix + "Ptt/SerialStopBits", config.pttSerialStopBits).toString();
    return config;
}
bool CatSettings::setGlobal(const CatGlobalConfig& config)
{
    // From Thetis setup.Designer.cs:59524-59528 [v2.10.3.15].
    // Native validation preserves rig selections and RTTY offset limits.
    if (config.rttyDiguHz < CatDefaults::kRttyMinimumHz || config.rttyDiguHz > CatDefaults::kRttyMaximumHz
        || config.rttyDiglHz < CatDefaults::kRttyMinimumHz || config.rttyDiglHz > CatDefaults::kRttyMaximumHz
        || !QStringList{"PowerSDR", "TS-2000", "TS-50S", "TS-480"}.contains(config.rigIdentity)
        || config.pttChannel < 1 || config.pttChannel > 4
        || !QStringList{"None", "CAT1", "CAT2", "CAT3", "CAT4", "Physical"}.contains(config.pttDeviceSource)
        || (config.pttEnabled && (config.pttDeviceSource == "None" || (!config.pttUseCts && !config.pttUseDsr)))
        || (config.pttEnabled && config.pttDeviceSource == "Physical" && config.pttSerialDevice.trimmed().isEmpty())
        || !serialFormat(config.pttSerialBaud, config.pttSerialParity, config.pttSerialDataBits, config.pttSerialStopBits)) { return false; }
    AppSettings& settings = m_settings;
    const QString prefix = QStringLiteral("Cat/");
    settings.setValue(prefix + "SendWelcome", boolString(config.sendWelcome));
    settings.setValue(prefix + "RigIdentity", config.rigIdentity);
    settings.setValue(prefix + "AllowKenwoodAi", boolString(config.allowKenwoodAi));
    settings.setValue(prefix + "AiEnabled", boolString(config.aiEnabled));
    settings.setValue(prefix + "AiSerial1", boolString(config.aiSerial1));
    settings.setValue(prefix + "AiSerial2", boolString(config.aiSerial2));
    settings.setValue(prefix + "AiSerial3", boolString(config.aiSerial3));
    settings.setValue(prefix + "AiSerial4", boolString(config.aiSerial4));
    settings.setValue(prefix + "AiTcp", boolString(config.aiTcp));
    settings.setValue(prefix + "DigitalReportsSideband", boolString(config.digitalReportsSideband));
    settings.setValue(prefix + "RecenterVfo", boolString(config.recenterVfo));
    settings.setValue(prefix + "SerialNumber", config.serialNumber);
    settings.setValue(prefix + "LimitReportedPower", boolString(config.limitReportedPower));
    settings.setValue(prefix + "RttyOffsetAEnabled", boolString(config.rttyOffsetAEnabled));
    settings.setValue(prefix + "RttyOffsetBEnabled", boolString(config.rttyOffsetBEnabled));
    settings.setValue(prefix + "RttyDiguHz", config.rttyDiguHz);
    settings.setValue(prefix + "RttyDiglHz", config.rttyDiglHz);
    settings.setValue(prefix + "Ptt/Enabled", boolString(config.pttEnabled));
    settings.setValue(prefix + "Ptt/DeviceSource", config.pttDeviceSource);
    settings.setValue(prefix + "Ptt/SerialDevice", config.pttSerialDevice);
    settings.setValue(prefix + "Ptt/UseCts", boolString(config.pttUseCts));
    settings.setValue(prefix + "Ptt/UseDsr", boolString(config.pttUseDsr));
    settings.setValue(prefix + "Ptt/Channel", config.pttChannel);
    settings.setValue(prefix + "Ptt/SerialBaud", config.pttSerialBaud);
    settings.setValue(prefix + "Ptt/SerialParity", config.pttSerialParity);
    settings.setValue(prefix + "Ptt/SerialDataBits", config.pttSerialDataBits);
    settings.setValue(prefix + "Ptt/SerialStopBits", config.pttSerialStopBits);
    return true;
}
} // namespace NereusSDR
