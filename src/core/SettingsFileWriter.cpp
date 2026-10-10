// =================================================================
// src/core/SettingsFileWriter.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  One writer thread for the settings
// file, and the signal that reports each background save's result.
// =================================================================

#include "SettingsFileWriter.h"

#include <utility>

namespace NereusSDR {

SettingsFileWriter::SettingsFileWriter(QObject* parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
    // The thread stays for the next save instead of being started again.
    m_pool.setExpiryTimeout(-1);
}

SettingsFileWriter::~SettingsFileWriter()
{
    m_pool.waitForDone();
}

void SettingsFileWriter::run(std::function<void()> job)
{
    m_pool.start(std::move(job));
}

void SettingsFileWriter::waitUntilIdle()
{
    m_pool.waitForDone();
}

void SettingsFileWriter::report(quint64 ticket, bool saved, const QString& error)
{
    emit finished(ticket, saved, error);
}

} // namespace NereusSDR
