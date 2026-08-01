#include "BuildIdentity.h"

namespace NereusSDR {
namespace BuildIdentity {

// Set once by main() from the generated build/generated/NereusBuildTag.h,
// read once by MainWindow::buildUI() when it composes the window title.
// Empty is the release answer and also the answer for any binary that never
// sets it, so no caller has to special-case a missing tag.
static QString s_buildTag;

void setBuildTag(const QString& tag)
{
    s_buildTag = tag;
}

QString buildTag()
{
    return s_buildTag;
}

} // namespace BuildIdentity
} // namespace NereusSDR
