# About Dialog Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the bare `QMessageBox::about()` with a proper About dialog crediting the open-source projects and people NereusSDR stands on.

**Architecture:** New `AboutDialog` QDialog subclass following the SupportDialog pattern — constructor + `buildUI()`, dark theme, no external dependencies. Wired into the existing Help menu action in MainWindow.

**Tech Stack:** C++20, Qt6 Widgets (QDialog, QLabel, QVBoxLayout, QHBoxLayout, QDesktopServices)

---

### Task 1: Create AboutDialog class with header section

**Files:**
- Create: `src/gui/AboutDialog.h`
- Create: `src/gui/AboutDialog.cpp`

- [ ] **Step 1: Create the header file**

```cpp
// src/gui/AboutDialog.h
#pragma once

#include <QDialog>

namespace NereusSDR {

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

private:
    void buildUI();
};

} // namespace NereusSDR
```

- [ ] **Step 2: Create the implementation with header section**

```cpp
// src/gui/AboutDialog.cpp
#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QPushButton>

namespace NereusSDR {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("About NereusSDR"));
    setFixedWidth(420);

    buildUI();

    // Size to content, prevent horizontal resize
    adjustSize();
    setFixedSize(size());
}

void AboutDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    setStyleSheet(QStringLiteral(
        "QDialog { background: #0f0f1a; }"
        "QLabel { color: #c8d8e8; }"
        "QLabel a { color: #00b4d8; text-decoration: none; }"
    ));

    // ── Header ──────────────────────────────────────────────────────────
    auto* icon = new QLabel(this);
    QPixmap pix(QStringLiteral(":/icons/NereusSDR.png"));
    if (!pix.isNull())
        icon->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(icon);
    mainLayout->addSpacing(8);

    auto* title = new QLabel(QStringLiteral("NereusSDR"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "font-size: 22px; font-weight: bold; color: #00b4d8;"));
    mainLayout->addWidget(title);

    auto* version = new QLabel(
        QStringLiteral("v%1 — Cross-platform SDR Console")
            .arg(QCoreApplication::applicationVersion()),
        this);
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 13px;"));
    mainLayout->addWidget(version);

    auto* buildInfo = new QLabel(
        QStringLiteral("Qt %1 · C++20 · Built %2")
            .arg(QString::fromUtf8(qVersion()),
                 QStringLiteral(__DATE__)),
        this);
    buildInfo->setAlignment(Qt::AlignCenter);
    buildInfo->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
    mainLayout->addWidget(buildInfo);

    mainLayout->addSpacing(4);

    auto* author = new QLabel(QStringLiteral("Created by JJ Boyd · KG4VCF"), this);
    author->setAlignment(Qt::AlignCenter);
    author->setStyleSheet(QStringLiteral("color: #aabbcc; font-size: 12px;"));
    mainLayout->addWidget(author);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* div1 = new QFrame(this);
    div1->setFrameShape(QFrame::HLine);
    div1->setStyleSheet(QStringLiteral(
        "QFrame { color: #334455; margin-top: 12px; margin-bottom: 12px; }"));
    mainLayout->addWidget(div1);

    // ── Standing on the Shoulders of Giants ─────────────────────────────
    auto* giantsHeading = new QLabel(
        QStringLiteral("Standing on the Shoulders of Giants"), this);
    giantsHeading->setStyleSheet(QStringLiteral(
        "color: #00b4d8; font-size: 13px; font-weight: 600;"));
    mainLayout->addWidget(giantsHeading);
    mainLayout->addSpacing(6);

    struct Contributor {
        const char* name;
        const char* callsign;
        const char* project;
        const char* url;
    };

    static constexpr std::array kContributors = {
        Contributor{"Richie",              "MW0LGE",  "Thetis",
                    "https://github.com/ramdor/Thetis"},
        Contributor{"Jeremy",              "KK7GWY",  "AetherSDR",
                    "https://github.com/ten9876/AetherSDR"},
        Contributor{"Reid Campbell",       "MI0BOT",  "OpenHPSDR-Thetis (HL2)",
                    "https://github.com/mi0bot/OpenHPSDR-Thetis"},
        Contributor{"Dr. Warren C. Pratt", "NR0V",    "WDSP",
                    "https://github.com/TAPR/OpenHPSDR-wdsp"},
        Contributor{"John Melton",         "G0ORX",   "WDSP Linux Port",
                    "https://github.com/g0orx/wdsp"},
    };

    for (const auto& c : kContributors) {
        auto* row = new QLabel(
            QStringLiteral("%1, %2 — <a href=\"%3\">%4</a>")
                .arg(QString::fromUtf8(c.name),
                     QString::fromUtf8(c.callsign),
                     QString::fromUtf8(c.url),
                     QString::fromUtf8(c.project)),
            this);
        row->setOpenExternalLinks(true);
        row->setStyleSheet(QStringLiteral(
            "color: #99aabb; font-size: 12px; padding: 2px 0;"));
        mainLayout->addWidget(row);
    }

    // TAPR line (no individual callsign)
    auto* taprRow = new QLabel(
        QStringLiteral("TAPR / <a href=\"https://github.com/TAPR\">"
                       "OpenHPSDR</a> Community"),
        this);
    taprRow->setOpenExternalLinks(true);
    taprRow->setStyleSheet(QStringLiteral(
        "color: #99aabb; font-size: 12px; padding: 2px 0;"));
    mainLayout->addWidget(taprRow);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* div2 = new QFrame(this);
    div2->setFrameShape(QFrame::HLine);
    div2->setStyleSheet(QStringLiteral(
        "QFrame { color: #334455; margin-top: 12px; margin-bottom: 12px; }"));
    mainLayout->addWidget(div2);

    // ── Built With ──────────────────────────────────────────────────────
    auto* builtHeading = new QLabel(QStringLiteral("Built With"), this);
    builtHeading->setStyleSheet(QStringLiteral(
        "color: #00b4d8; font-size: 13px; font-weight: 600;"));
    mainLayout->addWidget(builtHeading);
    mainLayout->addSpacing(6);

    struct Library {
        const char* name;
        const char* desc;
    };

    static constexpr std::array kLibraries = {
        Library{"Qt 6",       "GUI, Networking & Audio"},
        Library{"FFTW3",      "Spectrum & Waterfall FFT"},
        Library{"WDSP v1.29", "DSP Engine"},
    };

    auto* cardRow = new QHBoxLayout();
    cardRow->setSpacing(10);

    for (const auto& lib : kLibraries) {
        auto* card = new QLabel(
            QStringLiteral("<b>%1</b><br><span style=\"color:#aabbcc\">%2</span>")
                .arg(QString::fromUtf8(lib.name),
                     QString::fromUtf8(lib.desc)),
            this);
        card->setAlignment(Qt::AlignCenter);
        card->setStyleSheet(QStringLiteral(
            "background: #1a2a3a; border-radius: 6px; padding: 10px 8px;"
            " font-size: 11px; color: #c8d8e8;"));
        card->setMinimumWidth(110);
        cardRow->addWidget(card);
    }

    mainLayout->addLayout(cardRow);

    // ── Divider ─────────────────────────────────────────────────────────
    auto* div3 = new QFrame(this);
    div3->setFrameShape(QFrame::HLine);
    div3->setStyleSheet(QStringLiteral(
        "QFrame { color: #334455; margin-top: 12px; margin-bottom: 12px; }"));
    mainLayout->addWidget(div3);

    // ── Footer ──────────────────────────────────────────────────────────
    auto* copyright = new QLabel(QStringLiteral("© 2026 JJ Boyd"), this);
    copyright->setAlignment(Qt::AlignCenter);
    copyright->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(copyright);

    auto* license = new QLabel(
        QStringLiteral("Licensed under "
                       "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">"
                       "GPLv3</a>"),
        this);
    license->setAlignment(Qt::AlignCenter);
    license->setOpenExternalLinks(true);
    license->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(license);

    auto* repoLink = new QLabel(
        QStringLiteral("<a href=\"https://github.com/boydsoftprez/NereusSDR\">"
                       "github.com/boydsoftprez/NereusSDR</a>"),
        this);
    repoLink->setAlignment(Qt::AlignCenter);
    repoLink->setOpenExternalLinks(true);
    repoLink->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(repoLink);

    auto* hpsdr = new QLabel(
        QStringLiteral("HPSDR protocol © TAPR"), this);
    hpsdr->setAlignment(Qt::AlignCenter);
    hpsdr->setStyleSheet(QStringLiteral("color: #667788; font-size: 11px;"));
    mainLayout->addWidget(hpsdr);

    mainLayout->addSpacing(12);

    // ── OK button ───────────────────────────────────────────────────────
    auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
    okBtn->setDefault(true);
    okBtn->setFixedWidth(80);
    okBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #00b4d8; color: #0f0f1a; border-radius: 4px;"
        " padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: #33c8e8; }"));
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
}

} // namespace NereusSDR
```

- [ ] **Step 3: Commit new files**

```bash
git add src/gui/AboutDialog.h src/gui/AboutDialog.cpp
git commit -m "feat: add AboutDialog class with full layout"
```

---

### Task 2: Wire AboutDialog into the build and Help menu

**Files:**
- Modify: `CMakeLists.txt` — add `src/gui/AboutDialog.cpp` to `GUI_SOURCES`
- Modify: `src/gui/MainWindow.cpp` — replace `QMessageBox::about()` with `AboutDialog`
- Modify: `src/gui/MainWindow.h` — add `#include` (not strictly needed if included in .cpp only)

- [ ] **Step 1: Add AboutDialog.cpp to GUI_SOURCES in CMakeLists.txt**

In `CMakeLists.txt`, find the `set(GUI_SOURCES` block. After the existing `src/gui/SupportDialog.cpp` line (~line 278), add:

```
    src/gui/AboutDialog.cpp
```

- [ ] **Step 2: Replace QMessageBox::about() in MainWindow.cpp**

In `src/gui/MainWindow.cpp`, add the include at the top with the other dialog includes:

```cpp
#include "AboutDialog.h"
```

Then replace lines ~1274-1281 (the `QMessageBox::about` lambda):

Old:
```cpp
    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("About NereusSDR"),
            QString(QStringLiteral("NereusSDR v%1\n\n"
                                   "Cross-platform SDR Console\n"
                                   "Qt %2\n\n"
                                   "github.com/boydsoftprez/NereusSDR"))
                .arg(QCoreApplication::applicationVersion(), QString::fromUtf8(qVersion())));
    });
```

New:
```cpp
    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
```

- [ ] **Step 3: Build and verify**

```bash
cd build && cmake --build . --parallel
```

Expected: clean build, no errors.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/gui/MainWindow.cpp
git commit -m "feat: wire AboutDialog into build and Help menu"
```

---

### Task 3: Test — verify AboutDialog instantiates and contains expected content

**Files:**
- Create: `tests/tst_about_dialog.cpp`
- Modify: `tests/CMakeLists.txt` — register the new test

- [ ] **Step 1: Write the test**

```cpp
// tests/tst_about_dialog.cpp
#include <QtTest/QtTest>
#include <QLabel>
#include <QPushButton>
#include "gui/AboutDialog.h"

using namespace NereusSDR;

class TstAboutDialog : public QObject
{
    Q_OBJECT

private slots:
    void dialogInstantiates()
    {
        AboutDialog dlg;
        QCOMPARE(dlg.windowTitle(), QStringLiteral("About NereusSDR"));
    }

    void hasAuthorCredit()
    {
        AboutDialog dlg;
        // Find a QLabel containing the author credit
        bool found = false;
        const auto labels = dlg.findChildren<QLabel*>();
        for (const auto* lbl : labels) {
            if (lbl->text().contains(QStringLiteral("JJ Boyd"))) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Expected a QLabel containing 'JJ Boyd'");
    }

    void hasContributorLinks()
    {
        AboutDialog dlg;
        const auto labels = dlg.findChildren<QLabel*>();

        // Check for each contributor's project link
        QStringList expected = {
            QStringLiteral("ramdor/Thetis"),
            QStringLiteral("ten9876/AetherSDR"),
            QStringLiteral("mi0bot/OpenHPSDR-Thetis"),
            QStringLiteral("TAPR/OpenHPSDR-wdsp"),
            QStringLiteral("g0orx/wdsp"),
        };

        for (const auto& slug : expected) {
            bool found = false;
            for (const auto* lbl : labels) {
                if (lbl->text().contains(slug)) {
                    found = true;
                    break;
                }
            }
            QVERIFY2(found, qPrintable(QStringLiteral("Missing link for %1").arg(slug)));
        }
    }

    void hasLibraryCards()
    {
        AboutDialog dlg;
        const auto labels = dlg.findChildren<QLabel*>();

        QStringList libs = {
            QStringLiteral("Qt 6"),
            QStringLiteral("FFTW3"),
            QStringLiteral("WDSP v1.29"),
        };

        for (const auto& lib : libs) {
            bool found = false;
            for (const auto* lbl : labels) {
                if (lbl->text().contains(lib)) {
                    found = true;
                    break;
                }
            }
            QVERIFY2(found, qPrintable(QStringLiteral("Missing library card: %1").arg(lib)));
        }
    }

    void okButtonCloses()
    {
        AboutDialog dlg;
        auto* okBtn = dlg.findChild<QPushButton*>();
        QVERIFY(okBtn);
        QCOMPARE(okBtn->text(), QStringLiteral("OK"));

        // Clicking OK should accept the dialog
        QTest::mouseClick(okBtn, Qt::LeftButton);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }
};

QTEST_MAIN(TstAboutDialog)
#include "tst_about_dialog.moc"
```

- [ ] **Step 2: Register test in tests/CMakeLists.txt**

Add at the end of `tests/CMakeLists.txt`:

```cmake
# ── About dialog ─────────────────────────────────────────────────────────
nereus_add_test(tst_about_dialog)
```

- [ ] **Step 3: Build with tests and run**

```bash
cd build && cmake .. -DNEREUS_BUILD_TESTS=ON && cmake --build . --parallel
ctest --test-dir . -R tst_about_dialog -V
```

Expected: all 5 tests pass.

- [ ] **Step 4: Commit**

```bash
git add tests/tst_about_dialog.cpp tests/CMakeLists.txt
git commit -m "test: add AboutDialog content verification tests"
```

---

### Task 4: Smoke-test — launch app and verify dialog visually

- [ ] **Step 1: Launch the app**

```bash
cd build && ./NereusSDR &
```

- [ ] **Step 2: Open Help → About NereusSDR**

Verify visually:
- Dialog appears with dark theme (#0f0f1a background)
- App icon, title "NereusSDR", version, build info, "Created by JJ Boyd · KG4VCF"
- "Standing on the Shoulders of Giants" section with 6 entries
- Project names are clickable (open browser)
- "Built With" card row: Qt 6, FFTW3, WDSP v1.29
- Footer: copyright, GPLv3 link, GitHub link, HPSDR protocol line
- OK button closes dialog

- [ ] **Step 3: Run the full test suite to check for regressions**

```bash
ctest --test-dir build -V
```

Expected: all tests pass, no regressions.

- [ ] **Step 4: Commit any visual tweaks if needed**

If spacing, font sizes, or alignment needs adjustment after visual inspection, fix and commit:

```bash
git add -u
git commit -m "fix: polish AboutDialog spacing and alignment"
```
