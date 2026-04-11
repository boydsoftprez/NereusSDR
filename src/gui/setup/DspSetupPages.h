#pragma once

// src/gui/setup/DspSetupPages.h
//
// Nine DSP setup pages for the NereusSDR settings dialog.
// All controls are present as real widgets but disabled (NYI — no WDSP
// wiring yet).  Each class corresponds to one leaf item in the DSP
// category of the tree navigation.
//
// Pages:
//   AgcAlcSetupPage   — AGC / ALC / Leveler
//   NrAnfSetupPage    — Noise Reduction / ANF / EMNR
//   NbSnbSetupPage    — NB1 / NB2 / SNB
//   CwSetupPage       — Keyer / Timing / APF
//   AmSamSetupPage    — AM TX / SAM / Squelch
//   FmSetupPage       — FM RX / FM TX
//   VoxDexpSetupPage  — VOX / DEXP
//   CfcSetupPage      — CFC / Profile
//   MnfSetupPage      — Manual Notch Filter

#include "gui/SetupPage.h"

namespace NereusSDR {

class RadioModel;

// ── AGC / ALC ─────────────────────────────────────────────────────────────────

class AgcAlcSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit AgcAlcSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── NR / ANF ──────────────────────────────────────────────────────────────────

class NrAnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit NrAnfSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── NB / SNB ──────────────────────────────────────────────────────────────────

class NbSnbSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit NbSnbSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── CW ───────────────────────────────────────────────────────────────────────

class CwSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit CwSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── AM / SAM ─────────────────────────────────────────────────────────────────

class AmSamSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit AmSamSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── FM ───────────────────────────────────────────────────────────────────────

class FmSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit FmSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── VOX / DEXP ───────────────────────────────────────────────────────────────

class VoxDexpSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit VoxDexpSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── CFC ──────────────────────────────────────────────────────────────────────

class CfcSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit CfcSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── MNF ──────────────────────────────────────────────────────────────────────

class MnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
