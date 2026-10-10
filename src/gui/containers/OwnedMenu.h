// =================================================================
// src/gui/containers/OwnedMenu.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original heap-owned container context menu.
//
// NereusSDR original. A context menu that its owner may be destroyed
// under. A container's menu runs its own event loop, and an action in
// it (Return all objects and close shell, or Container Settings and
// Remove) can delete the container, its shell and every child of
// them before the menu returns. A menu on the stack is one of those
// children, and deleting it frees a stack address ("pointer being
// freed was not allocated", JJ 2026-10-09). This menu lives on the
// heap under its owner, so the owner's destruction deletes it cleanly;
// otherwise it is deleted when the holder goes out of scope.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09 - New. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#pragma once

#include <QMenu>
#include <QPointer>
#include <QWidget>

namespace NereusSDR {

// Holds a menu created on the heap under `owner` and deletes it when the
// holder goes out of scope, unless the owner took it down first:
//
//     OwnedMenu owned(this);
//     QMenu& menu = owned.menu();
//     ...
//     menu.exec(at);   // `this` may be gone when this returns
class OwnedMenu
{
public:
    explicit OwnedMenu(QWidget* owner)
        : m_menu(new QMenu(owner))
    {
    }
    ~OwnedMenu()
    {
        if (m_menu) {
            m_menu->deleteLater();
        }
    }
    OwnedMenu(const OwnedMenu&) = delete;
    OwnedMenu& operator=(const OwnedMenu&) = delete;

    QMenu& menu() const { return *m_menu; }

private:
    QPointer<QMenu> m_menu;
};

} // namespace NereusSDR
