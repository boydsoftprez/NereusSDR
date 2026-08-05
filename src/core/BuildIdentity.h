#pragma once

#include <QString>

namespace NereusSDR {

// Identity of the binary itself, as distinct from the product version.
//
// A smoke build stamps "branch@shortsha" (plus "-dirty" when it was built
// from a modified working tree) into the window title, so an operator on a
// bench can tell which build is on screen. Release artifacts carry no tag.
//
// Why this is injected at startup rather than compiled in wherever it is
// read: the tag is re-derived on every build from cmake/NereusBuildTag.cmake,
// so whatever translation unit consumes the generated header recompiles each
// time HEAD moves. MainWindow lives in NereusSDRObjs, which all ~450 test
// executables link, so consuming it there would relink the entire test suite
// on every commit. main.cpp is compiled into the application target alone,
// so it takes the hit (one small TU, one link) and hands the value here.
//
// Call setBuildTag() from main() BEFORE constructing MainWindow; the title is
// composed once, in MainWindow::buildUI(). Anything that never calls it (test
// binaries, for instance) simply reads back an empty tag and gets the plain
// untagged title, which is the correct answer for those.
namespace BuildIdentity {

void    setBuildTag(const QString& tag);
QString buildTag();

} // namespace BuildIdentity
} // namespace NereusSDR
