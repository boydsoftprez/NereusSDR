#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

namespace NereusSDR {

AppSettings& AppSettings::instance()
{
    static AppSettings s;
    return s;
}

AppSettings::AppSettings()
{
#ifdef Q_OS_MAC
    m_filePath = QDir::homePath() + "/Library/Preferences/NereusSDR/NereusSDR.settings";
#else
    m_filePath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                 + "/NereusSDR/NereusSDR.settings";
#endif
}

void AppSettings::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "No settings file found at" << m_filePath << "— using defaults";
        return;
    }

    QXmlStreamReader xml(&file);
    QString currentStation;
    bool inStation = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString tag = xml.name().toString();
            if (tag == "NereusSDR") {
                continue;
            }
            // Check if this is a station section
            if (!inStation && xml.attributes().hasAttribute(QStringLiteral("type"))
                && xml.attributes().value(QStringLiteral("type")) == QStringLiteral("station")) {
                inStation = true;
                currentStation = tag;
                m_stationName = tag;
                continue;
            }
            // Read element text as value
            const QString value = xml.readElementText();
            if (inStation) {
                m_stationSettings.insert(tag, value);
            } else {
                m_settings.insert(tag, value);
            }
        } else if (xml.isEndElement() && inStation) {
            if (xml.name().toString() == currentStation) {
                inStation = false;
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "XML parse error in settings:" << xml.errorString();
    }

    qDebug() << "Loaded" << m_settings.size() << "settings,"
             << m_stationSettings.size() << "station settings";
}

void AppSettings::save()
{
    // Ensure directory exists
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not save settings to" << m_filePath;
        return;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("NereusSDR");

    // Write top-level settings
    for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
        xml.writeTextElement(it.key(), it.value());
    }

    // Write station settings
    if (!m_stationSettings.isEmpty()) {
        xml.writeStartElement(m_stationName);
        xml.writeAttribute("type", "station");
        for (auto it = m_stationSettings.constBegin(); it != m_stationSettings.constEnd(); ++it) {
            xml.writeTextElement(it.key(), it.value());
        }
        xml.writeEndElement();
    }

    xml.writeEndElement(); // NereusSDR
    xml.writeEndDocument();

    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QVariant AppSettings::value(const QString& key, const QVariant& defaultValue) const
{
    auto it = m_settings.constFind(key);
    if (it != m_settings.constEnd()) {
        return QVariant(it.value());
    }
    return defaultValue;
}

void AppSettings::setValue(const QString& key, const QVariant& val)
{
    m_settings.insert(key, val.toString());
}

void AppSettings::remove(const QString& key)
{
    m_settings.remove(key);
}

bool AppSettings::contains(const QString& key) const
{
    return m_settings.contains(key);
}

QStringList AppSettings::allKeys() const
{
    return m_settings.keys();
}

QVariant AppSettings::stationValue(const QString& key, const QVariant& defaultValue) const
{
    auto it = m_stationSettings.constFind(key);
    if (it != m_stationSettings.constEnd()) {
        return QVariant(it.value());
    }
    return defaultValue;
}

void AppSettings::setStationValue(const QString& key, const QVariant& val)
{
    m_stationSettings.insert(key, val.toString());
}

QString AppSettings::stationName() const
{
    return m_stationName;
}

void AppSettings::setStationName(const QString& name)
{
    m_stationName = name;
}

} // namespace NereusSDR
