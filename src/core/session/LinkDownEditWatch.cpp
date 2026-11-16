// =================================================================
// src/core/session/LinkDownEditWatch.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - Created for the settings persistence audit. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/LinkDownEditWatch.h"

#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/StateMirror.h"
#include "models/SliceModel.h"

#include <QCoreApplication>
#include <QEvent>

#include <algorithm>

namespace NereusSDR {

LinkDownEditWatch::LinkDownEditWatch(QObject* parent)
    : QObject(parent)
    , m_mirror(new StateMirror(this))
{
    connect(m_mirror, &StateMirror::propertiesChanged, this, &LinkDownEditWatch::onChanged);
}

LinkDownEditWatch::~LinkDownEditWatch()
{
    setRecording(false);
}

void LinkDownEditWatch::start(const QHash<QByteArray, QPointer<QObject>>& objects)
{
    m_objects = objects;
    m_armed = true;
    setRecording(false);
    setRecording(true);
}

void LinkDownEditWatch::pause()
{
    setRecording(false);
}

void LinkDownEditWatch::resume()
{
    if (m_armed) {
        setRecording(true);
    }
}

void LinkDownEditWatch::clear()
{
    setRecording(false);
    m_armed = false;
    m_objects.clear();
    m_edits.clear();
}

QStringList LinkDownEditWatch::takeSuperseded()
{
    QStringList lost;
    for (auto object = m_edits.cbegin(); object != m_edits.cend(); ++object) {
        const QObject* live = m_objects.value(object.key()).data();
        if (live == nullptr) {
            continue;  // The Core no longer has it; nothing holds a value.
        }
        const MirrorSchema& schema = MirrorSchema::forObject(live);
        for (auto edit = object.value().cbegin(); edit != object.value().cend(); ++edit) {
            const MirrorProperty* property = schema.byName(edit.key());
            if (property != nullptr && schema.read(*property, live) != edit.value()) {
                lost.append(QStringLiteral("%1.%2")
                                .arg(QString::fromUtf8(object.key()),
                                     QString::fromUtf8(edit.key())));
            }
        }
    }
    clear();
    std::sort(lost.begin(), lost.end());
    return lost;
}

bool LinkDownEditWatch::eventFilter(QObject* watched, QEvent* event)
{
    // A drag is a mouse move with a button held. The held state is kept here
    // because reading it off the event would take a GUI header.
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
        m_buttonDown = true;
        m_sinceInput.start();
        break;
    case QEvent::MouseButtonRelease:
        m_buttonDown = false;
        m_sinceInput.start();
        break;
    case QEvent::MouseMove:
        if (m_buttonDown) {
            m_sinceInput.start();
        }
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::Shortcut:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::NativeGesture:
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
        m_sinceInput.start();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void LinkDownEditWatch::onChanged(const QByteArray& objectKey, const QList<MirrorUpdate>& updates)
{
    if (!inputIsRecent()) {
        return;
    }
    const QObject* object = m_objects.value(objectKey).data();
    if (object == nullptr) {
        return;
    }
    // A slice this window only listens to holds its changes back itself.
    if (const auto* slice = qobject_cast<const SliceModel*>(object);
        slice != nullptr && slice->isReadOnlyListener()) {
        return;
    }
    const QByteArray className = MirrorSchema::shortClassName(object->metaObject()->className());
    for (const MirrorUpdate& update : updates) {
        if (MirrorPolicy::inboundAllowed(className, update.name)) {
            m_edits[objectKey].insert(update.name, update.value);
        }
    }
}

void LinkDownEditWatch::setRecording(bool recording)
{
    if (recording == m_recording) {
        return;
    }
    m_recording = recording;
    m_buttonDown = false;
    m_sinceInput.invalidate();
    if (recording) {
        // Only while the link is down: every event in the window passes here.
        if (QCoreApplication* app = QCoreApplication::instance()) {
            app->installEventFilter(this);
        }
        for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
            if (it.value() != nullptr) {
                m_mirror->watch(it.key(), it.value().data());
            }
        }
    } else {
        if (QCoreApplication* app = QCoreApplication::instance()) {
            app->removeEventFilter(this);
        }
        m_mirror->unwatchAll();
    }
}

bool LinkDownEditWatch::inputIsRecent() const
{
    return m_sinceInput.isValid() && m_sinceInput.elapsed() <= kInputReachMs;
}

} // namespace NereusSDR
