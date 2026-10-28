// =================================================================
// src/gui/containers/ContainerWorkspaceNotices.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. What the operator is told when a
// container change is not saved.
//
// ContainerWorkspaceStore refuses a commit with a status and a reason. A
// one-off refusal is a toast. Two states refuse every commit until the
// layout is read again: the saved layout failed to load, or the saved
// layout changed under the store. For those the notice stays on screen
// and says what to do. A refused new container (Containers > New
// Container, or New and Duplicate in Container Settings) also says why in
// a message box. The store only reports; MainWindow and the Container
// Settings dialog show these words.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Written for NereusSDR by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code. A refused container change
//                 says why, and the stuck states say what to do.
// =================================================================

#pragma once

#include <QCoreApplication>
#include <QString>

namespace NereusSDR {

class ContainerWorkspaceNotices {
    Q_DECLARE_TR_FUNCTIONS(ContainerWorkspaceNotices)

public:
    /// The toast for one refused change, for example a save that lost a
    /// race. Later changes can still be saved.
    static QString changeNotSaved(const QString& reason)
    {
        return tr("Container change not saved: %1").arg(sentence(reason));
    }

    /// What to do while every container change is refused.
    static QString whatToDo()
    {
        return tr("To save container changes again, open Container Settings from a "
                  "container's menu and press Reload conflicting containers, or "
                  "restart NereusSDR.");
    }

    /// The notice that stays on screen while every change is refused.
    static QString readOnly(const QString& reason)
    {
        return tr("Container layout is read-only: %1. %2").arg(sentence(reason), whatToDo());
    }

    /// The message box title for a refused new container.
    static QString newContainerTitle() { return tr("New Container"); }

    /// The message box text for a refused new container. `stuck`: every
    /// change is refused until the layout is read again.
    static QString newContainerRefused(const QString& reason, bool stuck)
    {
        const QString why = reason.trimmed().isEmpty()
            ? tr("The new container could not be created.")
            : tr("The new container could not be saved: %1.").arg(sentence(reason));
        return stuck ? why + QLatin1Char(' ') + whatToDo() : why;
    }

private:
    // A reason is a phrase; the notices add their own full stop.
    static QString sentence(const QString& reason)
    {
        QString text = reason.trimmed();
        while (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
        return text;
    }
};

} // namespace NereusSDR
